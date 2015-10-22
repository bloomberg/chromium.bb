// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/clients/crw_js_injection_network_client.h"

#include "base/logging.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram.h"
#import "ios/net/crn_http_url_response.h"
#import "ios/third_party/blink/src/html_tokenizer.h"

// CRWJSInjectionNetworkClient injects an external script tag reference for
// crweb.js into HTML and XHTML documents. To do this correctly, three data
// points are needed: where to inject the script tag, what encoding the content
// is in (ASCII compatible, UTF32, UTF16 and big or little endian) and the byte
// length of the injection tag itself.
//
// The content encoding is handled first. As data is received,
// CRWJSInjectionNetworkClient will look at the beginning few bytes of the
// content for either a byte-order mark or an XML declaration. If present, they
// will be matched to a known set of patterns to determine the encoding. If not
// present, by definition, the content is in an ASCII compatible encoding (at
// least with regard to the markup tags themselves, which is all
// CRWJSInjectionNetworkClient cares about).
//
// Next, CRWJSInjectionNetworkClient will look for the byte offset at which to
// inject. For HTML and XHTML documents to work correctly, the script tag must
// be injected after the "<html>" tag (right after the '>' character). To
// determine the byte offset of this character, blink's HTMLTokenizer is used.
// Specifically, as data is received from the network,
// CRWJSInjectionNetworkClient will buffer the data until a sufficient amount of
// data is collected to scan for the "<html>" tag. This scan is repeated, as
// data is received, until one of three things happens: 1) an "<html>" tag is
// found, 2) some tag OTHER than an "<html>" tag is found, or 3) no tag at all
// is found within the first 1024 bytes of the content.
//
// For case 3) above, nothing is injected and the NSURLHTTPResponse as well as
// all associated data are immediately dispatched in unaltered form to their
// intended recipient (WebKit). All subsequently received data is simply passed
// along to its destination unaltered.
//
// For cases 1) & 2) above, the "Content-Length" header of the NSURLHTTPResponse
// object will be updated as appropriate (since NSURLHTTPResponse is unmutable,
// this means a copy is made) and then dispatched. Immediately after this, the
// data is dispatched in three chunks:
//  - Everything before the injection byte offset.
//  - The appropriately content encoded <script> tag string.
//  - All remaining data.
//
// All subsequently received data is simply passed along to its destination
// unaltered.
//

namespace {

// When looking for the meta-charset tag, blink/webkit
// limits itself to the first 1024 bytes.
const size_t kLimitOfBytesToCheckForHeadTag = 1024;
const size_t kMinimumBytesNeededForHTMLTag = 7;
const size_t kMinimumBytesNeededForBOM = 4;
const size_t kMinimumBytesNeededForXMLDecl = 8;

NSString* const kContentLength = @"Content-Length";

NSString* const kJSContentTemplate =
    @"<script src=\"%@_crweb.js\" charset=\"utf-8\"></script>";

// Compares a value pointed to by |bytes| to a single value |byte|. Returns
// true if they are equal, false otherwise. This is a base case for the
// variadic template version of the method of the same name below.
template <typename T1, typename T2>
bool BytesEqual(const T1* bytes, T2 byte) {
  DCHECK(bytes);
  return (*bytes == byte);
}

// Compares an array of values |bytes| to a value |byte| and a variable number
// of other values specified by |args|. Returns true if the values pointed to
// by |bytes| are equal to values |byte| through |args| in positional order,
// false otherwise. This means that if true is returned, values bytes[0...N] and
// byte...args are all equal.
template <typename T1, typename T2, typename... Arguments>
bool BytesEqual(const T1* bytes, T2 byte, Arguments... args) {
  DCHECK(bytes);
  if (*bytes == byte)
    return BytesEqual(++bytes, args...);

  return false;
}

// Gets the value corresponding to the key "Content-Length" in the passed
// |allHeaders| dictionary. If no such key is present in |allHeaders| returns
// -1.
long long GetContentLengthFromAllHeaders(NSDictionary* all_headers) {
  NSObject* content_length_object = [all_headers objectForKey:kContentLength];

  // This handles both the case when the object is an NSNumber and the case
  // when the object is an NSString.
  if ([content_length_object respondsToSelector:@selector(longLongValue)])
    return [static_cast<id>(content_length_object) longLongValue];

  return -1;
}

// Returns an CRNHTTPURLResponse instance with the "Content-Length"
// increased to include the passed |additionContentSize|. If
// |additionContentSize| is zero, the passed response is returned.
CRNHTTPURLResponse* ResponseWithUpdatedContentSize(
    CRNHTTPURLResponse* response,
    NSUInteger addition_content_size) {
  if (!response)
    return nil;

  if (!addition_content_size)
    return response;

  if (![response isKindOfClass:[CRNHTTPURLResponse class]]) {
    NOTREACHED();
    return response;
  }

  // NSURLResponse uses a long long return type for expectedContentLength.
  NSDictionary* all_headers = [response allHeaderFields];
  long long content_length = GetContentLengthFromAllHeaders(all_headers);
  if (content_length < 0) {
    return response;
  }

  // Create a new content length value.
  content_length += addition_content_size;
  NSString* content_length_value =
      [[NSNumber numberWithLongLong:content_length] stringValue];

  base::scoped_nsobject<NSMutableDictionary> all_headers_mutable;
  all_headers_mutable.reset([all_headers mutableCopy]);
  [all_headers_mutable setObject:content_length_value forKey:kContentLength];

  CRNHTTPURLResponse* update_response =
      [[CRNHTTPURLResponse alloc] initWithURL:[response URL]
                                   statusCode:[response statusCode]
                                  HTTPVersion:[response cr_HTTPVersion]
                                 headerFields:all_headers_mutable];

  return [update_response autorelease];
}
}  // namespace

@interface CRWJSInjectionNetworkClient () {
  // The CRNHTTPURLResponse that is held until it is determined if the content
  // will have JavaScript injected.
  base::scoped_nsobject<CRNHTTPURLResponse> _pendingResponse;

  // An array of data that is buffered until a determination is made about
  // injecting JavaScript.
  base::scoped_nsobject<NSMutableArray> _pendingData;

  // The content that will be injected in NSData form.
  base::scoped_nsobject<NSData> _jsInjectionContent;

  BOOL _completedByteOrderMarkCheck;
  BOOL _completedXMLDeclarationCheck;
  BOOL _completedCheckForWhereToInject;

  NSStringEncoding _contentEncoding;
  NSUInteger _headerLength;
  NSUInteger _pendingDataLength;

  NSUInteger _byteOffsetAtWhichToInject;
  BOOL _proceedWithInjection;
}

// Returns YES if all checks (BOM, XML declaration, injection location) are
// complete.
- (BOOL)completedAllChecks;

// Records a UMA histogram indicating the injection result
// (see enum InjectionResult).
- (void)recordHistogramResult;

// Returns an NSData containing the correctly encoded script tag referencing
// the appropriate crweb.js for this web view.
- (NSData*)jsInjectionContent;

// If injection is appropriate for this content, dispatches buffered data in
// the form:
//  1) [everything PRIOR to injection byte offset]
//  2) [self jsInjectionContent]
//  3) [everything AFTER the injection byte offset]
- (void)sendInjectedResponseIfNeeded;

// Dispatches the buffered and appropriately "Content-Length"-header-updated
// NSURLHTTPResponse to the next level network client (almost certainly
// WebKit/CFNetwork itself).
- (void)sendPendingResponse;

// Calls [self sendInjectedResponseIfNeeded] and then sends any remaining
// buffered data to the next level network client.
- (void)sendPendingData;

// Coalesces data contained in self.pendingData until a single NSData object of
// at least length |lengthNeeded| has been created and inserted into
// self.pendingData as the first element.
- (void)coalesceDataIfNeeded:(NSUInteger)lengthNeeded;

// Looks for a byte order mark in the content to indicate character encoding.
// Sets _completedByteOrderMarkCheck to YES once complete and updates
// _contentEncoding if an encoding has been determined.
- (void)checkForByteOrderMark;

// Looks for an XML declaration in the content to indicate character encoding.
// Sets _completedXMLDeclarationCheck to YES once complete and updates
// _contentEncoding if an encoding has been determined.
- (void)checkForXMLDeclaration;

// Checks whether the hard byte limit specified by
// kLimitOfBytesToCheckForHeadTag has been hit or exceeded during the check
// for an appropriate injection location. If the limit is hit or exceeded,
// _completedCheckForWhereToInject will be set to YES and
// -[CRWJSInjectionNetworkClient checkForWhereToInject will stop.
- (void)checkIfByteLimitPassed:(const WebCore::CharacterProvider&)provider;

// Checks for an appropriate byte offset at which to inject the external
// script tag. Sets _completedCheckForWhereToInject to YES once complete and
// updates _byteOffsetAtWhichToInject to the location at which to inject. Since
// this may be set to zero, _proceedWithInjection is set to YES to indicate that
// a location at which to inject has been found. Uses blink's HTMLTokenizer.
- (void)checkForWhereToInject;
@end

@implementation CRWJSInjectionNetworkClient

+ (BOOL)canHandleResponse:(NSURLResponse*)response {
  NSString* scheme = [[response URL] scheme];
  if (![scheme isEqualToString:@"http"] && ![scheme isEqualToString:@"https"])
    return NO;

  NSString* mimeType = [response MIMEType];

  if ([mimeType isEqualToString:@"text/html"])
    return YES;

  if ([mimeType isEqualToString:@"application/xhtml+xml"])
    return YES;

  return NO;
}

#pragma mark CRNNetworkClientProtocol implementation

- (void)didFailWithNSErrorCode:(NSInteger)nsErrorCode
                  netErrorCode:(int)netErrorCode {
  _proceedWithInjection = NO;
  [self sendPendingResponse];
  [self sendPendingData];
  [super didFailWithNSErrorCode:nsErrorCode netErrorCode:netErrorCode];
}

- (void)didLoadData:(NSData*)data {
  if ([self completedAllChecks]) {
    [super didLoadData:data];
    return;
  }

  if (!_pendingData)
    _pendingData.reset([[NSMutableArray alloc] init]);

  [_pendingData addObject:data];
  _pendingDataLength += [data length];

  [self checkForByteOrderMark];
  [self checkForXMLDeclaration];
  [self checkForWhereToInject];

  if ([self completedAllChecks]) {
    if (!_contentEncoding)
      _contentEncoding = NSASCIIStringEncoding;
    [self sendPendingResponse];
    [self sendPendingData];
  }
}

- (void)didReceiveResponse:(NSURLResponse*)response {
  DCHECK([response isKindOfClass:[CRNHTTPURLResponse class]]);
  DCHECK([response expectedContentLength] ==
         GetContentLengthFromAllHeaders(
             [static_cast<NSHTTPURLResponse*>(response) allHeaderFields]));
  DCHECK([CRWJSInjectionNetworkClient canHandleResponse:response]);

  // If response does not come with Content-Length header field (i.e. the
  // Transfer-Encoding header field has value 'chunked'), response does not have
  // to be updated.
  if ([response expectedContentLength] == -1) {
    [super didReceiveResponse:response];
  } else {
  // Client calls [super didReceiveResponse:] in sendPendingResponse.
    _pendingResponse.reset([static_cast<CRNHTTPURLResponse*>(response) retain]);
  }
}

- (void)didFinishLoading {
  [self recordHistogramResult];
  [self sendPendingResponse];
  [self sendPendingData];
  [super didFinishLoading];
}

#pragma mark Internal methods

- (BOOL)completedAllChecks {
  if (!_completedByteOrderMarkCheck)
    return NO;

  if (!_completedXMLDeclarationCheck)
    return NO;

  if (!_completedCheckForWhereToInject)
    return NO;

  return YES;
}

- (void)recordHistogramResult {
  web::InjectionResult result = web::InjectionResult::INJECTION_RESULT_COUNT;
  if (_proceedWithInjection)
    result = web::InjectionResult::SUCCESS_INJECTED;
  else if (_pendingDataLength < kMinimumBytesNeededForHTMLTag)
    result = web::InjectionResult::FAIL_INSUFFICIENT_CONTENT_LENGTH;
  else
    result = web::InjectionResult::FAIL_FIND_INJECTION_LOCATION;

  if (result < web::InjectionResult::INJECTION_RESULT_COUNT) {
    UMA_HISTOGRAM_ENUMERATION(
        "NetworkLayerJSInjection.Result", static_cast<int>(result),
        static_cast<int>(web::InjectionResult::INJECTION_RESULT_COUNT));
  } else {
    NOTREACHED();
  }
}

- (NSData*)jsInjectionContent {
  DCHECK(_proceedWithInjection);
  if (_jsInjectionContent)
    return _jsInjectionContent;

  NSString* jsContentString = [NSString
      stringWithFormat:kJSContentTemplate, [[NSUUID UUID] UUIDString]];
  _jsInjectionContent.reset(
      [[jsContentString dataUsingEncoding:_contentEncoding] retain]);

  return _jsInjectionContent;
}

- (void)sendInjectedResponseIfNeeded {
  if (!_proceedWithInjection)
    return;

  NSData* firstData = [_pendingData firstObject];
  NSUInteger dataLength = [firstData length];
  if (!dataLength)
    return;

  const uint8* bytes = reinterpret_cast<const uint8*>([firstData bytes]);

  // Construct one data in which to send the content + injected script tag.
  base::scoped_nsobject<NSMutableData> combined([[NSMutableData alloc] init]);
  if (_byteOffsetAtWhichToInject)
    [combined appendBytes:static_cast<const void*>(bytes)
                   length:_byteOffsetAtWhichToInject];

  // Send back the JavaScript content to inject.
  [combined appendData:[self jsInjectionContent]];
  [combined
      appendBytes:static_cast<const void*>(bytes + _byteOffsetAtWhichToInject)
           length:(dataLength - _byteOffsetAtWhichToInject)];

  [super didLoadData:combined];

  // The first data, into which the JS was injected, is no longer needed.
  [_pendingData.get() removeObjectAtIndex:0];
  _jsInjectionContent.reset();
}

- (void)sendPendingResponse {
  if (!_pendingResponse)
    return;

  if (_proceedWithInjection) {
    NSUInteger additionalLength = [[self jsInjectionContent] length];
    CRNHTTPURLResponse* responseToSend =
        ResponseWithUpdatedContentSize(_pendingResponse, additionalLength);
    _pendingResponse.reset([responseToSend retain]);
  }

  [super didReceiveResponse:_pendingResponse];
  _pendingResponse.reset();
}

- (void)sendPendingData {
  if (![_pendingData count])
    return;

  [self sendInjectedResponseIfNeeded];

  for (NSData* data in _pendingData.get())
    [super didLoadData:data];

  _pendingData.reset();
}

- (void)coalesceDataIfNeeded:(NSUInteger)lengthNeeded {
  NSData* firstData = [_pendingData firstObject];

  // Obviously if the first data object has enough data,
  // nothing needs to be done.
  if ([firstData length] >= lengthNeeded)
    return;

  // Make sure we have something that can be coalesced.
  if ([_pendingData count] < 2)
    return;

  // Start with a mutable copy of the first data item.
  base::scoped_nsobject<NSMutableData> coalescedData([firstData mutableCopy]);

  // Replace the first item in the array.
  [_pendingData removeObjectAtIndex:0];

  // While not enough data has been coalesced and there are items
  // that can be coalesced, do so.
  while ([coalescedData length] < lengthNeeded && [_pendingData count]) {
    [coalescedData appendData:_pendingData[0]];
    [_pendingData removeObjectAtIndex:0];
  }

  // Finally, put the coalesced object at the front of the pending data array.
  [_pendingData insertObject:coalescedData atIndex:0];
}

- (void)checkForByteOrderMark {
  // Bail if the check has already been completed.
  if (_completedByteOrderMarkCheck)
    return;

  // Bail if there is not yet enough data to check.
  if (_pendingDataLength < kMinimumBytesNeededForBOM)
    return;

  // It's possible that the server returned data in small chunks, so coalesce
  // the data into one buffer if needed.
  [self coalesceDataIfNeeded:kMinimumBytesNeededForBOM];

  // Get some data to investigate.
  NSData* firstData = [_pendingData firstObject];
  if (!firstData && [firstData length] < kMinimumBytesNeededForBOM) {
    NOTREACHED();
    return;
  }

  // Do the same check that WebKit does for the byte order mark (BOM), which
  // must be right at the beginning of the content to be accepted.
  // Info on byte order mark: http://en.wikipedia.org/wiki/Byte_order_mark
  const uint8* bytes = reinterpret_cast<const uint8*>([firstData bytes]);
  if (BytesEqual(bytes, 0xFF, 0xFE)) {
    bytes += 2;

    if (!BytesEqual(bytes, 0x00, 0x00)) {
      _contentEncoding = NSUTF16LittleEndianStringEncoding;
      _headerLength += 2;
    } else {
      _contentEncoding = NSUTF32LittleEndianStringEncoding;
      _headerLength += 4;
    }
  } else if (BytesEqual(bytes, 0xEF, 0xBB, 0xBF)) {
    _contentEncoding = NSUTF8StringEncoding;
    _headerLength += 3;
  } else if (BytesEqual(bytes, 0xFE, 0xFF)) {
    _contentEncoding = NSUTF16BigEndianStringEncoding;
    _headerLength += 2;
  } else if (BytesEqual(bytes, 0x00, 0x00, 0xFE, 0xFF)) {
    _contentEncoding = NSUTF32BigEndianStringEncoding;
    _headerLength += 4;
  }

  _completedByteOrderMarkCheck = YES;
}

- (void)checkForXMLDeclaration {
  // WebKit interprets the byte order mark (BOM) as the truth and ignores and
  // subsequent encodings. Thus if a BOM has already been found, there is no
  // need to check for an XML declaration.
  if (_headerLength)
    _completedXMLDeclarationCheck = YES;

  // Bail if we have completed this.
  if (_completedXMLDeclarationCheck)
    return;

  // Do we already know the encoding?
  if (_contentEncoding) {
    _completedXMLDeclarationCheck = YES;
    return;
  }

  // Bail if there is not yet enough data to check.
  if (_pendingDataLength < kMinimumBytesNeededForXMLDecl)
    return;

  // It's possible that the server returned data in small chunks, so coalesce
  // the data into one buffer if needed.
  [self coalesceDataIfNeeded:kMinimumBytesNeededForXMLDecl];

  // Get some data to investigate.
  NSData* firstData = [_pendingData firstObject];
  if (!firstData && [firstData length] < kMinimumBytesNeededForXMLDecl) {
    NOTREACHED();
    return;
  }

  // Do the same check that WebKit does for an XML declaration. The XML spec
  // is not exactly clear about what, if anything, can appear before an XML
  // declaration. Can there be white space? Can there be comments? WebKit only
  // accepts XML declarations if they are right at the beginning of the content.
  const uint8* bytes = reinterpret_cast<const uint8*>([firstData bytes]);
  if (BytesEqual(bytes, '<', '?', 'x', 'm', 'l')) {
    _contentEncoding = NSISOLatin1StringEncoding;
  } else if (BytesEqual(bytes, '<', 0, '?', 0, 'x', 0)) {
    _contentEncoding = NSUTF16LittleEndianStringEncoding;
  } else if (BytesEqual(bytes, 0, '<', 0, '?', 0, 'x')) {
    _contentEncoding = NSUTF16BigEndianStringEncoding;
  } else if (BytesEqual(bytes, '<', 0, 0, 0, '?', 0, 0, 0)) {
    _contentEncoding = NSUTF32LittleEndianStringEncoding;
  } else if (BytesEqual(bytes, 0, 0, 0, '<', 0, 0, 0, '?')) {
    _contentEncoding = NSUTF32BigEndianStringEncoding;
  }

  _completedXMLDeclarationCheck = YES;
}

- (void)checkIfByteLimitPassed:(const WebCore::CharacterProvider&)provider {
  // Put a hard limit on the number of characters we check before doing
  // an injection.
  if (provider.bytesProvided() >= kLimitOfBytesToCheckForHeadTag)
    _completedCheckForWhereToInject = YES;
}

- (void)checkForWhereToInject {
  // Bail if the check has already been completed.
  if (_completedCheckForWhereToInject)
    return;

  // Bail if there is not yet enough data to check. We need a complete "<html "
  // or a complete "<html>"
  if (_pendingDataLength < kMinimumBytesNeededForHTMLTag)
    return;

  // It's possible that the server returned data in small chunks, so coalesce
  // the data into one buffer if needed. Try pooling everything into one chunk
  // that meets the limit of what we look at.
  [self coalesceDataIfNeeded:kLimitOfBytesToCheckForHeadTag];

  // Get some data to investigate. Of course we need at least
  // kMinimumBytesNeededForHTMLTag bytes to do the investigation.
  NSData* firstData = [_pendingData firstObject];
  DCHECK([firstData length] >= kMinimumBytesNeededForHTMLTag);

  const uint8* bytes8 = reinterpret_cast<const uint8*>([firstData bytes]);

  WebCore::CharacterProvider provider;
  switch (_contentEncoding) {
    case NSUTF16BigEndianStringEncoding:
    case NSUTF16LittleEndianStringEncoding:
    case NSUTF32BigEndianStringEncoding:
    case NSUTF32LittleEndianStringEncoding: {
      const uint16* bytes16 =
          reinterpret_cast<const uint16*>(bytes8 + _headerLength);

      provider.setContents(bytes16, [firstData length] - _headerLength);

      if (_contentEncoding == NSUTF16LittleEndianStringEncoding ||
          _contentEncoding == NSUTF32LittleEndianStringEncoding)
        provider.setLittleEndian();
      break;
    }

    default: {
      provider.setContents(bytes8 + _headerLength,
                           [firstData length] - _headerLength);
      break;
    }
  }

  WebCore::HTMLTokenizer tokenizer;
  WebCore::HTMLToken token;
  while(!_completedCheckForWhereToInject &&
        tokenizer.nextToken(provider, token)) {
    WebCore::HTMLToken::Type tokenType = token.type();
    switch (tokenType) {
      case WebCore::HTMLToken::StartTag: {
        DEFINE_STATIC_LOCAL_STRING(htmlTag, "html");

        // If any start tag is seen, the check is complete.
        _proceedWithInjection = YES;
        _completedCheckForWhereToInject = YES;

        // We always have to account for any BOM header when injecting.
        _byteOffsetAtWhichToInject = _headerLength;

        if (token.nameEquals(htmlTag, htmlTagLength)) {
          // There is an html tag, so the insertion needs
          // to happen after this start tag.
          _byteOffsetAtWhichToInject += provider.bytesProvided();
        }

        break;
      }

      default: {
        [self checkIfByteLimitPassed:provider];
        token.clear();
        break;
      }
    }
  }

  // There is an early exit case right at the end of the start-tag from the
  // WebCore::HTMLTokenizer::nextToken(), so double check to see if we hit
  // the limit.
  [self checkIfByteLimitPassed:provider];
}

@end
