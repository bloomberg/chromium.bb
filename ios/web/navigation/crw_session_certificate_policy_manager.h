// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_SESSION_CERTIFICATE_POLICY_MANAGER_H_
#define IOS_WEB_NAVIGATION_CRW_SESSION_CERTIFICATE_POLICY_MANAGER_H_

#import <Foundation/Foundation.h>
#include <string>

#include "base/memory/ref_counted.h"
#include "net/cert/cert_status_flags.h"

namespace net {
class X509Certificate;
}

namespace web {
class CertificatePolicyCache;
}

// The CRWSessionCertificatePolicyManager keeps track of the certificates that
// have been manually allowed by the user despite the errors.
// The CRWSessionCertificatePolicyManager lives on the main thread.
@interface CRWSessionCertificatePolicyManager : NSObject <NSCoding, NSCopying>

- (void)registerAllowedCertificate:
            (const scoped_refptr<net::X509Certificate>)certificate
                           forHost:(const std::string&)host
                            status:(net::CertStatus)status;

// Removes all the certificates associated with this session. Note that this has
// no effect on the policy cache service.
- (void)clearCertificates;

// Copies the certificate polices for the session into |cache|.
- (void)updateCertificatePolicyCache:
    (const scoped_refptr<web::CertificatePolicyCache>&)cache;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_SESSION_CERTIFICATE_POLICY_MANAGER_H_
