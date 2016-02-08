// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/error_translation_util.h"

#include <CFNetwork/CFNetwork.h>

#import "base/ios/ns_error_util.h"
#include "base/mac/scoped_nsobject.h"
#include "net/base/net_errors.h"

namespace web {

namespace {
// Translates an iOS error to a net error using |net_error_code| as an
// out-parameter.  Returns true if a valid translation was found.
bool GetNetErrorFromIOSErrorCode(NSInteger ios_error_code,
                                 NSInteger* net_error_code) {
  DCHECK(net_error_code);
  bool translation_success = true;
  switch (ios_error_code) {
    case kCFURLErrorBackgroundSessionInUseByAnotherProcess:
      *net_error_code = net::ERR_ACCESS_DENIED;
      break;
    case kCFURLErrorBackgroundSessionWasDisconnected:
      *net_error_code = net::ERR_CONNECTION_ABORTED;
      break;
    case kCFURLErrorUnknown:
      *net_error_code = net::ERR_FAILED;
      break;
    case kCFURLErrorCancelled:
      *net_error_code = net::ERR_ABORTED;
      break;
    case kCFURLErrorBadURL:
      *net_error_code = net::ERR_INVALID_URL;
      break;
    case kCFURLErrorTimedOut:
      *net_error_code = net::ERR_CONNECTION_TIMED_OUT;
      break;
    case kCFURLErrorUnsupportedURL:
      *net_error_code = net::ERR_UNKNOWN_URL_SCHEME;
      break;
    case kCFURLErrorCannotFindHost:
      *net_error_code = net::ERR_NAME_NOT_RESOLVED;
      break;
    case kCFURLErrorCannotConnectToHost:
      *net_error_code = net::ERR_CONNECTION_FAILED;
      break;
    case kCFURLErrorNetworkConnectionLost:
      *net_error_code = net::ERR_INTERNET_DISCONNECTED;
      break;
    case kCFURLErrorDNSLookupFailed:
      *net_error_code = net::ERR_NAME_RESOLUTION_FAILED;
      break;
    case kCFURLErrorHTTPTooManyRedirects:
      *net_error_code = net::ERR_TOO_MANY_REDIRECTS;
      break;
    case kCFURLErrorResourceUnavailable:
      *net_error_code = net::ERR_INSUFFICIENT_RESOURCES;
      break;
    case kCFURLErrorNotConnectedToInternet:
      *net_error_code = net::ERR_INTERNET_DISCONNECTED;
      break;
    case kCFURLErrorRedirectToNonExistentLocation:
      *net_error_code = net::ERR_NAME_NOT_RESOLVED;
      break;
    case kCFURLErrorBadServerResponse:
      *net_error_code = net::ERR_INVALID_RESPONSE;
      break;
    case kCFURLErrorUserCancelledAuthentication:
      *net_error_code = net::ERR_ABORTED;
      break;
    case kCFURLErrorUserAuthenticationRequired:
      // TODO(crbug.com/546159): ERR_SSL_RENEGOTIATION_REQUESTED is more
      // specific than the kCFURLErrorUserAuthenticationRequired.  Consider
      // adding a new net error for this scenario.
      *net_error_code = net::ERR_SSL_RENEGOTIATION_REQUESTED;
      break;
    case kCFURLErrorZeroByteResource:
      *net_error_code = net::ERR_EMPTY_RESPONSE;
      break;
    case kCFURLErrorCannotDecodeRawData:
      *net_error_code = net::ERR_CONTENT_DECODING_FAILED;
      break;
    case kCFURLErrorCannotDecodeContentData:
      *net_error_code = net::ERR_CONTENT_DECODING_FAILED;
      break;
    case kCFURLErrorCannotParseResponse:
      *net_error_code = net::ERR_INVALID_RESPONSE;
      break;
    case kCFURLErrorInternationalRoamingOff:
      // TODO(crbug.com/546165): Create new net error for disabled intl roaming.
      *net_error_code = net::ERR_INTERNET_DISCONNECTED;
      break;
    case kCFURLErrorCallIsActive:
      *net_error_code = net::ERR_CONNECTION_FAILED;
      break;
    case kCFURLErrorDataNotAllowed:
      // TODO(crbug.com/546167): Create new net error for disabled data usage.
      *net_error_code = net::ERR_INTERNET_DISCONNECTED;
      break;
    case kCFURLErrorRequestBodyStreamExhausted:
      *net_error_code = net::ERR_CONTENT_LENGTH_MISMATCH;
      break;
    case kCFURLErrorFileDoesNotExist:
      *net_error_code = net::ERR_FILE_NOT_FOUND;
      break;
    case kCFURLErrorFileIsDirectory:
      *net_error_code = net::ERR_INVALID_HANDLE;
      break;
    case kCFURLErrorNoPermissionsToReadFile:
      *net_error_code = net::ERR_ACCESS_DENIED;
      break;
    case kCFURLErrorDataLengthExceedsMaximum:
      *net_error_code = net::ERR_FILE_TOO_BIG;
      break;
    case kCFURLErrorSecureConnectionFailed:
      *net_error_code = net::ERR_SSL_PROTOCOL_ERROR;
      break;
    case kCFURLErrorServerCertificateHasBadDate:
      *net_error_code = net::ERR_CERT_DATE_INVALID;
      break;
    case kCFURLErrorServerCertificateUntrusted:
      *net_error_code = net::ERR_CERT_AUTHORITY_INVALID;
      break;
    case kCFURLErrorServerCertificateHasUnknownRoot:
      *net_error_code = net::ERR_CERT_AUTHORITY_INVALID;
      break;
    case kCFURLErrorServerCertificateNotYetValid:
      *net_error_code = net::ERR_CERT_DATE_INVALID;
      break;
    case kCFURLErrorClientCertificateRejected:
      *net_error_code = net::ERR_BAD_SSL_CLIENT_AUTH_CERT;
      break;
    case kCFURLErrorClientCertificateRequired:
      *net_error_code = net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED;
      break;
    default:
      translation_success = false;
      break;
  }
  return translation_success;
}
}  // namespace

NSError* NetErrorFromError(NSError* error) {
  DCHECK(error);
  NSError* underlying_error =
      base::ios::GetFinalUnderlyingErrorFromError(error);

  NSInteger net_error_code = net::ERR_FAILED;
  if ([underlying_error.domain isEqualToString:NSURLErrorDomain] ||
      [underlying_error.domain
          isEqualToString:static_cast<NSString*>(kCFErrorDomainCFNetwork)]) {
    // Attempt to translate NSURL and CFNetwork error codes into their
    // corresponding net error codes.
    GetNetErrorFromIOSErrorCode(underlying_error.code, &net_error_code);
  }

  NSString* net_error_domain =
      [NSString stringWithUTF8String:net::kErrorDomain];
  NSError* net_error = [NSError errorWithDomain:net_error_domain
                                           code:net_error_code
                                       userInfo:nil];
  return base::ios::ErrorWithAppendedUnderlyingError(error, net_error);
}

}  // namespace web
