// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/ios/block_types.h"
#include "base/memory/ref_counted.h"
#include "ios/web/public/cert_policy.h"
#include "net/cert/cert_status_flags.h"

namespace net {
class X509Certificate;
}

namespace web {
class CertificatePolicyCache;
}

// Adapter for web::CertificatePolicyCache, which is used to remember decisions
// about how to handle invalid certs that have been given a user exception.
// web::CertificatePolicyCache can be used only on IO thread while
// CRWCertPolicyCache is asynchronous and can be used on the main thread.
@interface CRWCertPolicyCache : NSObject

// Unavailable, use |initWithCache:| instead.
- (instancetype)init NS_UNAVAILABLE;

// Initializes CRWCertPolicyCache.
- (instancetype)initWithCache:(scoped_refptr<web::CertificatePolicyCache>)cache
    NS_DESIGNATED_INITIALIZER;

// Asynchronously queries whether |cert| with |status| is allowed or denied for
// |host|. |handler| can not be null and is always called on the main thread.
- (void)queryJudgementForCert:(scoped_refptr<net::X509Certificate>)cert
                      forHost:(NSString*)host
                       status:(net::CertStatus)certStatus
            completionHandler:(void (^)(web::CertPolicy::Judgment))handler;

// Asynchronously records that |cert| is permitted to be used for |host| in the
// future. |handler| can not be null and is always called on the main thread.
- (void)allowCert:(scoped_refptr<net::X509Certificate>)cert
              forHost:(NSString*)host
               status:(net::CertStatus)status
    completionHandler:(ProceduralBlock)handler;

@end
