// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/cronet/ios/Cronet.h"

#include <memory>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/scoped_block.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"
#include "components/cronet/ios/accept_languages_table.h"
#include "components/cronet/ios/cronet_environment.h"
#include "components/cronet/url_request_context_config.h"
#include "ios/net/crn_http_protocol_handler.h"
#include "ios/net/empty_nsurlcache.h"
#include "net/cert/cert_verifier.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

class CronetHttpProtocolHandlerDelegate;

// Currently there is one and only one instance of CronetEnvironment,
// which is leaked at the shutdown. We should consider allowing multiple
// instances if that makes sense in the future.
base::LazyInstance<std::unique_ptr<cronet::CronetEnvironment>>::Leaky
    gChromeNet = LAZY_INSTANCE_INITIALIZER;

BOOL gHttp2Enabled = YES;
BOOL gQuicEnabled = NO;
cronet::URLRequestContextConfig::HttpCacheType gHttpCache =
    cronet::URLRequestContextConfig::HttpCacheType::DISK;
ScopedVector<cronet::URLRequestContextConfig::QuicHint> gQuicHints;
NSString* gExperimentalOptions = @"{}";
NSString* gUserAgent = nil;
BOOL gUserAgentPartial = NO;
NSString* gSslKeyLogFileName = nil;
ScopedVector<cronet::URLRequestContextConfig::Pkp> gPkpList = {};
RequestFilterBlock gRequestFilterBlock = nil;
base::LazyInstance<std::unique_ptr<CronetHttpProtocolHandlerDelegate>>::Leaky
    gHttpProtocolHandlerDelegate = LAZY_INSTANCE_INITIALIZER;
NSURLCache* gPreservedSharedURLCache = nil;
BOOL gEnableTestCertVerifierForTesting = FALSE;
std::unique_ptr<net::CertVerifier> gMockCertVerifier;
NSString* gAcceptLanguages = nil;

// CertVerifier, which allows any certificates for testing.
class TestCertVerifier : public net::CertVerifier {
  int Verify(const RequestParams& params,
             net::CRLSet* crl_set,
             net::CertVerifyResult* verify_result,
             const net::CompletionCallback& callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override {
    net::Error result = net::OK;
    verify_result->verified_cert = params.certificate();
    verify_result->cert_status = net::MapNetErrorToCertStatus(result);
    return result;
  }
};

// net::HTTPProtocolHandlerDelegate for Cronet.
class CronetHttpProtocolHandlerDelegate
    : public net::HTTPProtocolHandlerDelegate {
 public:
  CronetHttpProtocolHandlerDelegate(net::URLRequestContextGetter* getter,
                                    RequestFilterBlock filter)
      : getter_(getter), filter_(filter, base::scoped_policy::RETAIN) {}

  void SetRequestFilterBlock(RequestFilterBlock filter) {
    base::AutoLock auto_lock(lock_);
    filter_.reset(filter, base::scoped_policy::RETAIN);
  }

 private:
  // net::HTTPProtocolHandlerDelegate implementation:
  bool CanHandleRequest(NSURLRequest* request) override {
    base::AutoLock auto_lock(lock_);
    if (!IsRequestSupported(request))
      return false;
    if (filter_) {
      RequestFilterBlock block = filter_.get();
      return block(request);
    }
    return true;
  }

  bool IsRequestSupported(NSURLRequest* request) override {
    NSString* scheme = [[request URL] scheme];
    if (!scheme)
      return false;
    return [scheme caseInsensitiveCompare:@"http"] == NSOrderedSame ||
           [scheme caseInsensitiveCompare:@"https"] == NSOrderedSame;
  }

  net::URLRequestContextGetter* GetDefaultURLRequestContext() override {
    return getter_.get();
  }

  scoped_refptr<net::URLRequestContextGetter> getter_;
  base::mac::ScopedBlock<RequestFilterBlock> filter_;
  base::Lock lock_;
};

}  // namespace

@implementation Cronet

+ (void)configureCronetEnvironmentForTesting:
    (cronet::CronetEnvironment*)cronetEnvironment {
  if (gEnableTestCertVerifierForTesting) {
    std::unique_ptr<TestCertVerifier> test_cert_verifier =
        base::MakeUnique<TestCertVerifier>();
    cronetEnvironment->set_mock_cert_verifier(std::move(test_cert_verifier));
  }
  if (gMockCertVerifier) {
    gChromeNet.Get()->set_mock_cert_verifier(std::move(gMockCertVerifier));
  }
}

+ (NSString*)getAcceptLanguagesFromPreferredLanguages:
    (NSArray<NSString*>*)languages {
  NSMutableArray* acceptLanguages = [NSMutableArray new];
  for (NSString* lang_region in languages) {
    NSString* lang = [lang_region componentsSeparatedByString:@"-"][0];
    NSString* localeAcceptLangs = acceptLangs[lang_region] ?: acceptLangs[lang];
    if (localeAcceptLangs)
      [acceptLanguages
          addObjectsFromArray:[localeAcceptLangs
                                  componentsSeparatedByString:@","]];
  }

  NSString* acceptLanguageString =
      [[[NSOrderedSet orderedSetWithArray:acceptLanguages] array]
          componentsJoinedByString:@","];

  return [acceptLanguageString length] != 0 ? acceptLanguageString
                                            : @"en-US,en";
}

+ (NSString*)getAcceptLanguages {
  return [self
      getAcceptLanguagesFromPreferredLanguages:[NSLocale preferredLanguages]];
}

+ (void)setAcceptLanguages:(NSString*)acceptLanguages {
  [self checkNotStarted];
  gAcceptLanguages = acceptLanguages;
}

// TODO(lilyhoughton) this should either be removed, or made more sophisticated
+ (void)checkNotStarted {
  CHECK(!gChromeNet.Get()) << "Cronet is already started.";
}

+ (void)setHttp2Enabled:(BOOL)http2Enabled {
  [self checkNotStarted];
  gHttp2Enabled = http2Enabled;
}

+ (void)setQuicEnabled:(BOOL)quicEnabled {
  [self checkNotStarted];
  gQuicEnabled = quicEnabled;
}

+ (void)addQuicHint:(NSString*)host port:(int)port altPort:(int)altPort {
  [self checkNotStarted];
  gQuicHints.push_back(new cronet::URLRequestContextConfig::QuicHint(
      base::SysNSStringToUTF8(host), port, altPort));
}

+ (void)setExperimentalOptions:(NSString*)experimentalOptions {
  [self checkNotStarted];
  gExperimentalOptions = experimentalOptions;
}

+ (void)setUserAgent:(NSString*)userAgent partial:(BOOL)partial {
  [self checkNotStarted];
  gUserAgent = userAgent;
  gUserAgentPartial = partial;
}

+ (void)setSslKeyLogFileName:(NSString*)sslKeyLogFileName {
  [self checkNotStarted];
  gSslKeyLogFileName = [self getNetLogPathForFile:sslKeyLogFileName];
}

+ (void)setHttpCacheType:(CRNHttpCacheType)httpCacheType {
  [self checkNotStarted];
  switch (httpCacheType) {
    case CRNHttpCacheTypeDisabled:
      gHttpCache = cronet::URLRequestContextConfig::HttpCacheType::DISABLED;
      break;
    case CRNHttpCacheTypeDisk:
      gHttpCache = cronet::URLRequestContextConfig::HttpCacheType::DISK;
      break;
    case CRNHttpCacheTypeMemory:
      gHttpCache = cronet::URLRequestContextConfig::HttpCacheType::MEMORY;
      break;
    default:
      DCHECK(NO) << "Invalid HTTP cache type: " << httpCacheType;
  }
}

+ (void)setRequestFilterBlock:(RequestFilterBlock)block {
  if (gHttpProtocolHandlerDelegate.Get().get())
    gHttpProtocolHandlerDelegate.Get().get()->SetRequestFilterBlock(block);
  else
    gRequestFilterBlock = block;
}

+ (BOOL)addPublicKeyPinsForHost:(NSString*)host
                      pinHashes:(NSSet<NSData*>*)pinHashes
              includeSubdomains:(BOOL)includeSubdomains
                 expirationDate:(NSDate*)expirationDate
                          error:(NSError**)outError {
  [self checkNotStarted];

  auto pkp = base::MakeUnique<cronet::URLRequestContextConfig::Pkp>(
      base::SysNSStringToUTF8(host), includeSubdomains,
      base::Time::FromCFAbsoluteTime(
          [expirationDate timeIntervalSinceReferenceDate]));

  for (NSData* hash in pinHashes) {
    net::SHA256HashValue hashValue = net::SHA256HashValue();
    if (sizeof(hashValue.data) != hash.length) {
      *outError =
          [self createIllegalArgumentErrorWithArgument:@"pinHashes"
                                                reason:
                                                    @"The length of PKP SHA256 "
                                                    @"hash should be 256 bits"];
      return NO;
    }
    memcpy((void*)(hashValue.data), [hash bytes], sizeof(hashValue.data));
    pkp->pin_hashes.push_back(net::HashValue(hashValue));
  }
  gPkpList.push_back(std::move(pkp));
  if (outError) {
    *outError = nil;
  }
  return YES;
}

+ (void)startInternal {
  std::string user_agent = base::SysNSStringToUTF8(gUserAgent);

  gChromeNet.Get().reset(
      new cronet::CronetEnvironment(user_agent, gUserAgentPartial));

  gChromeNet.Get()->set_accept_language(
      base::SysNSStringToUTF8(gAcceptLanguages ?: [self getAcceptLanguages]));

  gChromeNet.Get()->set_http2_enabled(gHttp2Enabled);
  gChromeNet.Get()->set_quic_enabled(gQuicEnabled);
  gChromeNet.Get()->set_experimental_options(
      base::SysNSStringToUTF8(gExperimentalOptions));
  gChromeNet.Get()->set_http_cache(gHttpCache);
  gChromeNet.Get()->set_ssl_key_log_file_name(
      base::SysNSStringToUTF8(gSslKeyLogFileName));
  gChromeNet.Get()->set_pkp_list(std::move(gPkpList));
  for (const auto* quicHint : gQuicHints) {
    gChromeNet.Get()->AddQuicHint(quicHint->host, quicHint->port,
                                  quicHint->alternate_port);
  }

  [self configureCronetEnvironmentForTesting:gChromeNet.Get().get()];
  gChromeNet.Get()->Start();
  gHttpProtocolHandlerDelegate.Get().reset(
      new CronetHttpProtocolHandlerDelegate(
          gChromeNet.Get()->GetURLRequestContextGetter(), gRequestFilterBlock));
  net::HTTPProtocolHandlerDelegate::SetInstance(
      gHttpProtocolHandlerDelegate.Get().get());
  gRequestFilterBlock = nil;
}

+ (void)start {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    if (![NSThread isMainThread]) {
      dispatch_sync(dispatch_get_main_queue(), ^(void) {
        cronet::CronetEnvironment::Initialize();
      });
    } else {
      cronet::CronetEnvironment::Initialize();
    }
  });

  [self startInternal];
}

+ (void)shutdownForTesting {
  gChromeNet.Get().reset();
}

+ (void)registerHttpProtocolHandler {
  if (gPreservedSharedURLCache == nil) {
    gPreservedSharedURLCache = [NSURLCache sharedURLCache];
  }
  // Disable the default cache.
  [NSURLCache setSharedURLCache:[EmptyNSURLCache emptyNSURLCache]];
  // Register the chrome http protocol handler to replace the default one.
  BOOL success =
      [NSURLProtocol registerClass:[CRNHTTPProtocolHandler class]];
  DCHECK(success);
}

+ (void)unregisterHttpProtocolHandler {
  // Set up SharedURLCache preserved in registerHttpProtocolHandler.
  if (gPreservedSharedURLCache != nil) {
    [NSURLCache setSharedURLCache:gPreservedSharedURLCache];
    gPreservedSharedURLCache = nil;
  }
  [NSURLProtocol unregisterClass:[CRNHTTPProtocolHandler class]];
}

+ (void)installIntoSessionConfiguration:(NSURLSessionConfiguration*)config {
  config.protocolClasses = @[ [CRNHTTPProtocolHandler class] ];
}

+ (NSString*)getNetLogPathForFile:(NSString*)fileName {
  return [[[[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory
                                                   inDomains:NSUserDomainMask]
      lastObject] URLByAppendingPathComponent:fileName] path];
}

+ (BOOL)startNetLogToFile:(NSString*)fileName logBytes:(BOOL)logBytes {
  if (gChromeNet.Get().get() && [fileName length] &&
      ![fileName isAbsolutePath]) {
    return gChromeNet.Get()->StartNetLog(
        base::SysNSStringToUTF8([self getNetLogPathForFile:fileName]),
        logBytes);
  }

  return NO;
}

+ (void)stopNetLog {
  if (gChromeNet.Get().get()) {
    gChromeNet.Get()->StopNetLog();
  }
}

+ (NSString*)getUserAgent {
  if (!gChromeNet.Get().get()) {
    return nil;
  }

  return [NSString stringWithCString:gChromeNet.Get()->user_agent().c_str()
                            encoding:[NSString defaultCStringEncoding]];
}

+ (stream_engine*)getGlobalEngine {
  DCHECK(gChromeNet.Get().get());
  if (gChromeNet.Get().get()) {
    static stream_engine engine;
    engine.obj = gChromeNet.Get()->GetURLRequestContextGetter();
    return &engine;
  }
  return nil;
}

+ (NSData*)getGlobalMetricsDeltas {
  if (!gChromeNet.Get().get()) {
    return nil;
  }
  std::vector<uint8_t> deltas(gChromeNet.Get()->GetHistogramDeltas());
  return [NSData dataWithBytes:deltas.data() length:deltas.size()];
}

+ (void)enableTestCertVerifierForTesting {
  gEnableTestCertVerifierForTesting = YES;
}

+ (void)setMockCertVerifierForTesting:
    (std::unique_ptr<net::CertVerifier>)certVerifier {
  gMockCertVerifier = std::move(certVerifier);
}

+ (void)setHostResolverRulesForTesting:(NSString*)hostResolverRulesForTesting {
  DCHECK(gChromeNet.Get().get());
  gChromeNet.Get()->SetHostResolverRules(
      base::SysNSStringToUTF8(hostResolverRulesForTesting));
}

// This is a non-public dummy method that prevents the linker from stripping out
// the otherwise non-referenced methods from 'bidirectional_stream.cc'.
+ (void)preventStrippingCronetBidirectionalStream {
  bidirectional_stream_create(NULL, 0, 0);
}

+ (NSError*)createIllegalArgumentErrorWithArgument:(NSString*)argumentName
                                            reason:(NSString*)reason {
  NSMutableDictionary* errorDictionary =
      [[NSMutableDictionary alloc] initWithDictionary:@{
        NSLocalizedDescriptionKey :
            [NSString stringWithFormat:@"Invalid argument: %@", argumentName],
        CRNInvalidArgumentKey : argumentName
      }];
  if (reason) {
    errorDictionary[NSLocalizedFailureReasonErrorKey] = reason;
  }
  return [self createCronetErrorWith:CRNErrorInvalidArgument
                            userInfo:errorDictionary];
}

+ (NSError*)createCronetErrorWith:(int)errorCode
                         userInfo:(NSDictionary*)userInfo {
  return [NSError errorWithDomain:CRNCronetErrorDomain
                             code:errorCode
                         userInfo:userInfo];
}

@end
