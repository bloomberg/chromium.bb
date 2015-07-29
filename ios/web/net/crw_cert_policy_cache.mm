// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/crw_cert_policy_cache.h"

#include "base/location.h"
#include "base/mac/bind_objc_block.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web/public/certificate_policy_cache.h"
#include "ios/web/public/web_thread.h"

@interface CRWCertPolicyCache () {
  // Used to remember decisions about how to handle problematic certs.
  scoped_refptr<web::CertificatePolicyCache> _impl;
}
@end

@implementation CRWCertPolicyCache

- (instancetype)init {
  NOTREACHED();
  return self;
}

- (instancetype)initWithCache:(scoped_refptr<web::CertificatePolicyCache>)impl {
  DCHECK(impl);
  self = [super init];
  if (self) {
    _impl = impl;
  }
  return self;
}

- (void)queryJudgementForCert:(scoped_refptr<net::X509Certificate>)cert
                      forHost:(NSString*)host
                       status:(net::CertStatus)status
            completionHandler:(void (^)(web::CertPolicy::Judgment))handler {
  DCHECK(handler);
  web::WebThread::PostTask(web::WebThread::IO, FROM_HERE, base::BindBlock(^{
    web::CertPolicy::Judgment policy =
        _impl->QueryPolicy(cert.get(), base::SysNSStringToUTF8(host), status);
    dispatch_async(dispatch_get_main_queue(), ^{
      handler(policy);
    });
  }));
}

- (void)allowCert:(scoped_refptr<net::X509Certificate>)cert
          forHost:(NSString*)host
           status:(net::CertStatus)status {
  web::WebThread::PostTask(web::WebThread::IO, FROM_HERE, base::BindBlock(^{
    _impl->AllowCertForHost(cert.get(), base::SysNSStringToUTF8(host), status);
  }));
}

@end
