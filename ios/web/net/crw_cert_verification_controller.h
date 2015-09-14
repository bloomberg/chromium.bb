// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_CRW_CERT_VERIFICATION_CONTROLLER_H_
#define IOS_WEB_NET_CRW_CERT_VERIFICATION_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "base/memory/ref_counted.h"
#include "net/cert/cert_status_flags.h"

namespace net {
class X509Certificate;
}

namespace web {

class BrowserState;

// Accept policy for valid or invalid SSL cert.
typedef NS_ENUM(NSInteger, CertAcceptPolicy) {
  // Cert status can't be determined due to an error. Caller should not proceed
  // with the load, but show net error page instead.
  CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR = 0,
  // Cert is not valid. Caller may present SSL warning and ask user if they
  // want to proceed with the load.
  CERT_ACCEPT_POLICY_RECOVERABLE_ERROR,
  // Cert is valid. Caller should proceed with the load.
  CERT_ACCEPT_POLICY_ALLOW,
};

// Completion handler called by decidePolicyForCert:host:completionHandler:.
typedef void (^PolicyDecisionHandler)(web::CertAcceptPolicy, net::CertStatus);

}  // namespace web

// Provides various cert verification API that can be used for blocking requests
// with bad SSL cert, presenting SSL interstitials and determining SSL status
// for Navigation Items. Must be used on UI thread.
@interface CRWCertVerificationController : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Initializes CRWCertVerificationController with the given |browserState| which
// cannot be null and must outlive CRWCertVerificationController.
- (instancetype)initWithBrowserState:(web::BrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

// TODO(eugenebut): add API for:
// - accepting bad SSL cert using CertPolicyCache
// - querying SSL cert status for Navigation Item

// Decides the policy for the given |cert| for the given |host| and calls
// |completionHandler| on completion. |completionHandler| cannot be null and
// will be called synchronously or asynchronously on UI thread.
- (void)decidePolicyForCert:(const scoped_refptr<net::X509Certificate>&)cert
                       host:(NSString*)host
          completionHandler:(web::PolicyDecisionHandler)handler;

// Cancels all pending verification requests. Completion handlers will not be
// called after |shutDown| call. Must always be called before object's
// deallocation.
- (void)shutDown;

@end

#endif  // IOS_WEB_NET_CRW_CERT_VERIFICATION_CONTROLLER_H_
