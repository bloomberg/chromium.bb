// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/physical_web/physical_web_request.h"

#include "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"
#import "ios/chrome/common/physical_web/physical_web_device.h"
#import "ios/chrome/common/physical_web/physical_web_types.h"
#include "ios/web/public/user_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef void (^SessionCompletionProceduralBlock)(NSData* data,
                                                 NSURLResponse* response,
                                                 NSError* error);

namespace {

NSString* const kUrlsKey = @"urls";
NSString* const kUrlKey = @"url";
NSString* const kResultsKey = @"results";
NSString* const kPageInfoKey = @"pageInfo";
NSString* const kIconKey = @"icon";
NSString* const kTitleKey = @"title";
NSString* const kDescriptionKey = @"description";
NSString* const kScannedUrlKey = @"scannedUrl";
NSString* const kResolvedUrlKey = @"resolvedUrl";

NSString* const kMetadataServiceUrl =
    @"https://physicalweb.googleapis.com/v1alpha1/urls:resolve";
NSString* const kKeyQueryItemName = @"key";
NSString* const kHTTPPOSTRequestMethod = @"POST";

std::string GetUserAgent() {
  static std::string user_agent;
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    std::string product("CriOS/");
    product += version_info::GetVersionNumber();
    user_agent = web::BuildUserAgentFromProduct(product);
  });
  return user_agent;
}

}  // namespace

@implementation PhysicalWebRequest {
  base::mac::ScopedBlock<physical_web::RequestFinishedBlock> block_;
  base::scoped_nsobject<PhysicalWebDevice> device_;
  base::scoped_nsobject<NSMutableURLRequest> request_;
  base::scoped_nsobject<NSURLSessionDataTask> urlSessionTask_;
  base::scoped_nsobject<NSMutableData> data_;
  base::scoped_nsobject<NSDate> startDate_;
}

- (instancetype)initWithDevice:(PhysicalWebDevice*)device {
  self = [super init];
  if (self) {
    device_.reset(device);
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (NSURL*)requestURL {
  return [device_ requestURL];
}

- (void)cancel {
  [urlSessionTask_ cancel];
  block_.reset();
}

- (void)start:(physical_web::RequestFinishedBlock)block {
  block_.reset([block copy]);
  data_.reset([[NSMutableData alloc] init]);

  // Creates the HTTP post request.
  base::scoped_nsobject<NSURLComponents> components(
      [[NSURLComponents alloc] initWithString:kMetadataServiceUrl]);
  NSString* apiKey =
      [NSString stringWithUTF8String:google_apis::GetAPIKey().c_str()];
  [components
      setQueryItems:@[ [NSURLQueryItem queryItemWithName:kKeyQueryItemName
                                                   value:apiKey] ]];
  NSURL* url = [components URL];
  request_.reset([NSMutableURLRequest requestWithURL:url]);
  [request_ setHTTPMethod:kHTTPPOSTRequestMethod];

  // body of the POST request.
  NSDictionary* jsonBody =
      @{ kUrlsKey : @[ @{kUrlKey : [[device_ requestURL] absoluteString]} ] };
  [request_ setHTTPBody:[NSJSONSerialization dataWithJSONObject:jsonBody
                                                        options:0
                                                          error:NULL]];
  [request_ setValue:@"application/json" forHTTPHeaderField:@"Content-Type"];
  [request_ setValue:base::SysUTF8ToNSString(GetUserAgent())
      forHTTPHeaderField:@"User-Agent"];

  // Set the Accept-Language header from the locale language code. This may
  // cause us to fetch metadata for the wrong region in languages such as
  // Chinese that vary significantly between regions.
  // TODO(mattreynolds): Use the same Accept-Language string as WKWebView.
  NSString* acceptLanguage =
      [[NSLocale currentLocale] objectForKey:NSLocaleLanguageCode];
  [request_ setValue:acceptLanguage forHTTPHeaderField:@"Acccept-Language"];

  startDate_.reset([NSDate date]);
  // Starts the request.
  NSURLSessionConfiguration* sessionConfiguration =
      [NSURLSessionConfiguration ephemeralSessionConfiguration];
  sessionConfiguration.HTTPCookieAcceptPolicy = NSHTTPCookieAcceptPolicyNever;
  NSURLSession* session =
      [NSURLSession sessionWithConfiguration:sessionConfiguration
                                    delegate:nil
                               delegateQueue:[NSOperationQueue mainQueue]];
  base::WeakNSObject<PhysicalWebRequest> weakSelf(self);
  SessionCompletionProceduralBlock completionHandler =
      ^(NSData* data, NSURLResponse* response, NSError* error) {
        base::scoped_nsobject<PhysicalWebRequest> strongSelf(weakSelf);
        if (!strongSelf) {
          return;
        }
        if (error) {
          [strongSelf callBlockWithError:error jsonObject:nil];
        } else {
          [strongSelf.get()->data_ appendData:data];
          [strongSelf sessionDidFinishLoading];
        }
      };
  urlSessionTask_.reset([session dataTaskWithRequest:request_
                                   completionHandler:completionHandler]);
  [urlSessionTask_ resume];
}

- (void)sessionDidFinishLoading {
  NSError* error = nil;
  NSDictionary* jsonObject =
      [NSJSONSerialization JSONObjectWithData:data_ options:0 error:&error];
  if (error != nil) {
    [self callBlockWithError:error jsonObject:nil];
    return;
  }
  if (![self isValidJSON:jsonObject]) {
    [self callBlockWithError:
              [NSError errorWithDomain:physical_web::kPhysicalWebErrorDomain
                                  code:physical_web::ERROR_INVALID_JSON
                              userInfo:nil]
                  jsonObject:jsonObject];
    return;
  }
  [self callBlockWithError:error jsonObject:jsonObject];
}

// Checks whether the JSON object has the correct format.
// The format should be similar to the following:
// {
//   "results":[
//     {
//       "pageInfo":{
//         "title":"Google"
//         "description":"Search the world's information"
//         "icon":"https://www.google.com/favicon.ico"
//       }
//       "scannedUrl":"https://www.google.com"
//       "resolvedUrl":"https://www.google.com"
//     }
//   ]
// }
- (BOOL)isValidJSON:(id)jsonObject {
  NSDictionary* dict = base::mac::ObjCCast<NSDictionary>(jsonObject);
  if (!dict) {
    return NO;
  }
  NSArray* list = base::mac::ObjCCast<NSArray>(dict[kResultsKey]);
  if (!list) {
    return NO;
  }
  if ([list count] != 1) {
    return NO;
  }
  NSDictionary* item = base::mac::ObjCCast<NSDictionary>(list[0]);
  if (!item) {
    return NO;
  }
  return YES;
}

// Calls the block passed as parameter of -start: when the request is finished.
- (void)callBlockWithError:(NSError*)error
                jsonObject:(NSDictionary*)jsonObject {
  if (error) {
    if (block_.get()) {
      block_.get()(nil, error);
    }
  } else {
    NSTimeInterval roundTripTime =
        [[NSDate date] timeIntervalSinceDate:startDate_];
    int roundTripTimeMS = 1000 * roundTripTime;
    UMA_HISTOGRAM_COUNTS("PhysicalWeb.RoundTripTimeMilliseconds",
                         roundTripTimeMS);
    NSArray* list = jsonObject[kResultsKey];
    NSDictionary* item = list[0];

    NSDictionary* pageInfo =
        base::mac::ObjCCast<NSDictionary>(item[kPageInfoKey]);
    NSString* scannedUrlString =
        base::mac::ObjCCast<NSString>(item[kScannedUrlKey]);
    NSString* resolvedUrlString =
        base::mac::ObjCCast<NSString>(item[kResolvedUrlKey]);

    // Verify required fields pageInfo, scannedUrl, and resolvedUrl are present.
    if (pageInfo == nil || scannedUrlString == nil ||
        resolvedUrlString == nil) {
      error = [NSError errorWithDomain:physical_web::kPhysicalWebErrorDomain
                                  code:physical_web::ERROR_INVALID_JSON
                              userInfo:nil];
      if (block_.get()) {
        block_.get()(nil, error);
      }
      return;
    }

    // Read optional fields.
    NSString* iconString = base::mac::ObjCCast<NSString>(pageInfo[kIconKey]);
    NSString* description =
        base::mac::ObjCCast<NSString>(pageInfo[kDescriptionKey]);
    NSString* title = base::mac::ObjCCast<NSString>(pageInfo[kTitleKey]);
    NSURL* resolvedUrl =
        resolvedUrlString ? [NSURL URLWithString:resolvedUrlString] : nil;
    NSURL* icon = iconString ? [NSURL URLWithString:iconString] : nil;
    base::scoped_nsobject<PhysicalWebDevice> device([[PhysicalWebDevice alloc]
          initWithURL:resolvedUrl
           requestURL:[device_ requestURL]
                 icon:icon
                title:title
          description:description
        transmitPower:[device_ transmitPower]
                 rssi:[device_ rssi]
                 rank:physical_web::kMaxRank
        scanTimestamp:[device_ scanTimestamp]]);
    if (block_.get() != nil) {
      block_.get()(device, nil);
    }
  }
}

@end
