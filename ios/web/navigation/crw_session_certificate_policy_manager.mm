// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_session_certificate_policy_manager.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web/public/certificate_policy_cache.h"
#include "ios/web/public/web_thread.h"
#include "net/base/hash_value.h"
#include "net/cert/x509_certificate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Break if we detect that CertStatus values changed, because we persist them on
// disk and thus require them to be consistent.
static_assert(net::CERT_STATUS_ALL_ERRORS == 0xFF00FFFF,
              "The value of CERT_STATUS_ALL_ERRORS changed!");
static_assert(net::CERT_STATUS_COMMON_NAME_INVALID == 1 << 0,
              "The value of CERT_STATUS_COMMON_NAME_INVALID changed!");
static_assert(net::CERT_STATUS_DATE_INVALID == 1 << 1,
              "The value of CERT_STATUS_DATE_INVALID changed!");
static_assert(net::CERT_STATUS_AUTHORITY_INVALID == 1 << 2,
              "The value of CERT_STATUS_AUTHORITY_INVALID changed!");
static_assert(net::CERT_STATUS_NO_REVOCATION_MECHANISM == 1 << 4,
              "The value of CERT_STATUS_NO_REVOCATION_MECHANISM changed!");
static_assert(net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION == 1 << 5,
              "The value of CERT_STATUS_UNABLE_TO_CHECK_REVOCATION changed!");
static_assert(net::CERT_STATUS_REVOKED == 1 << 6,
              "The value of CERT_STATUS_REVOKED changed!");
static_assert(net::CERT_STATUS_INVALID == 1 << 7,
              "The value of CERT_STATUS_INVALID changed!");
static_assert(net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM == 1 << 8,
              "The value of CERT_STATUS_WEAK_SIGNATURE_ALGORITHM changed!");
static_assert(net::CERT_STATUS_NON_UNIQUE_NAME == 1 << 10,
              "The value of CERT_STATUS_NON_UNIQUE_NAME changed!");
static_assert(net::CERT_STATUS_WEAK_KEY == 1 << 11,
              "The value of CERT_STATUS_WEAK_KEY changed!");
static_assert(net::CERT_STATUS_IS_EV == 1 << 16,
              "The value of CERT_STATUS_IS_EV changed!");
static_assert(net::CERT_STATUS_REV_CHECKING_ENABLED == 1 << 17,
              "The value of CERT_STATUS_REV_CHECKING_ENABLED changed!");

namespace {

NSString* const kAllowedCertificatesKey = @"allowedCertificates";

struct AllowedCertificate {
  scoped_refptr<net::X509Certificate> certificate;
  net::SHA256HashValue certificateHash;
  std::string host;
};

class LessThan {
 public:
  bool operator() (const AllowedCertificate& lhs,
                   const AllowedCertificate& rhs) const {
    if (lhs.host != rhs.host)
      return lhs.host < rhs.host;
    return hash_compare_(lhs.certificateHash, rhs.certificateHash);
  }
 private:
  net::SHA256HashValueLessThan hash_compare_;
};

typedef std::map<AllowedCertificate, net::CertStatus, LessThan>
    AllowedCertificates;

NSData* CertificateToNSData(net::X509Certificate* certificate) {
  std::string s;
  bool success =
      net::X509Certificate::GetDEREncoded(certificate->os_cert_handle(), &s);
  DCHECK(success);
  return [NSData dataWithBytes:s.c_str() length:s.length()];
}

scoped_refptr<net::X509Certificate> NSDataToCertificate(NSData* data) {
  return net::X509Certificate::CreateFromBytes((const char *)[data bytes],
                                               [data length]);
}

void AddToCertificatePolicyCache(
    scoped_refptr<web::CertificatePolicyCache> policy_cache,
    AllowedCertificates certs) {
  DCHECK(policy_cache);
  AllowedCertificates::iterator it;
  for (it = certs.begin(); it != certs.end(); ++it) {
    policy_cache->AllowCertForHost(
        it->first.certificate.get(), it->first.host, it->second);
  }
}

}  // namespace

@implementation CRWSessionCertificatePolicyManager {
 @private
  AllowedCertificates allowed_;
}

- (void)registerAllowedCertificate:
            (const scoped_refptr<net::X509Certificate>)certificate
                           forHost:(const std::string&)host
                            status:(net::CertStatus)status {
  DCHECK([NSThread isMainThread]);
  DCHECK(certificate);
  AllowedCertificate allowedCertificate = {
      certificate, net::X509Certificate::CalculateChainFingerprint256(
                       certificate->os_cert_handle(),
                       certificate->GetIntermediateCertificates()),
      host};
  allowed_[allowedCertificate] = status;
}

- (void)clearCertificates {
  DCHECK([NSThread isMainThread]);
  allowed_.clear();
}

- (void)updateCertificatePolicyCache:
    (const scoped_refptr<web::CertificatePolicyCache>&)cache {
  DCHECK([NSThread isMainThread]);
  DCHECK(cache);
  // Make a copy of allowed_ and access the policy cache from the IOThread.
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(&AddToCertificatePolicyCache, cache, allowed_));
}

#pragma mark -
#pragma mark NSCoding and NSCopying methods

- (id)initWithCoder:(NSCoder*)aDecoder {
  DCHECK([NSThread isMainThread]);
  self = [super init];
  if (self) {
    NSMutableSet* allowed = [aDecoder
        decodeObjectForKey:kAllowedCertificatesKey];
    for (NSArray* fields in allowed) {
      if ([fields count] == 2) {
        DVLOG(2) << "Dropping cached certificate policy (old format).";
        continue;
      } else if ([fields count] != 3) {
        NOTREACHED();
        continue;
      }
      scoped_refptr<net::X509Certificate> cert =
          NSDataToCertificate([fields objectAtIndex:0]);
      std::string host = base::SysNSStringToUTF8([fields objectAtIndex:1]);
      net::CertStatus status = (net::CertStatus)[[fields objectAtIndex:2]
          unsignedIntegerValue];
      [self registerAllowedCertificate:cert forHost:host status:status];
    }
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  if (allowed_.size() == 0)
    return;

  // Simple serialization of the set. If a same certificate is duplicated in the
  // set (for a different host), the serialization will be duplicated as well.
  NSMutableSet* allowedToEncode = [NSMutableSet set];
  AllowedCertificates::iterator it;
  for (it = allowed_.begin(); it != allowed_.end(); ++it) {
    NSData* c = CertificateToNSData(it->first.certificate.get());
    NSString* h = base::SysUTF8ToNSString(it->first.host);
    DCHECK(c);
    DCHECK(h);
    if (!c || !h)
      continue;
    const NSUInteger status = (NSUInteger)it->second;
    NSArray* fields = [NSArray arrayWithObjects:c, h, @(status), nil];
    [allowedToEncode addObject:fields];
  }
  [aCoder encodeObject:allowedToEncode forKey:kAllowedCertificatesKey];
}

- (id)copyWithZone:(NSZone*)zone {
  DCHECK([NSThread isMainThread]);
  CRWSessionCertificatePolicyManager* copy = [[[self class] alloc] init];
  copy->allowed_ = allowed_;
  return copy;
}

@end
