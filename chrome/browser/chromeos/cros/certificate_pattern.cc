// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/certificate_pattern.h"

#include <algorithm>
#include <list>
#include <string>
#include <vector>

#include <cert.h>
#include <pk11pub.h>

#include "net/base/cert_database.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"
#include "net/base/x509_cert_types.h"

// To shorten some of those long lines below.
using std::vector;
using std::string;
using std::list;
using std::find;

namespace chromeos {

namespace {
// Functor to filter out non-matching issuers.
class IssuerFilter {
 public:
  explicit IssuerFilter(const IssuerSubjectPattern& issuer)
    : issuer_(issuer) {}
  bool operator()(const scoped_refptr<net::X509Certificate>& cert) const {
    return !issuer_.Matches(cert.get()->issuer());
  }
 private:
  const IssuerSubjectPattern& issuer_;
};

// Functor to filter out non-matching subjects.
class SubjectFilter {
 public:
  explicit SubjectFilter(const IssuerSubjectPattern& subject)
    : subject_(subject) {}
  bool operator()(const scoped_refptr<net::X509Certificate>& cert) const {
    return !subject_.Matches(cert.get()->subject());
  }
 private:
  const IssuerSubjectPattern& subject_;
};

// Functor to filter out certs that don't have private keys, or are invalid.
class PrivateKeyFilter {
 public:
  explicit PrivateKeyFilter(net::CertDatabase* cert_db) : cert_db_(cert_db) {}
  bool operator()(const scoped_refptr<net::X509Certificate>& cert) const {
    return cert_db_->CheckUserCert(cert.get()) != net::OK;
  }
 private:
  net::CertDatabase* cert_db_;
};

// Functor to filter out certs that don't have an issuer in the associated
// IssuerCARef list.
class IssuerCaRefFilter {
 public:
  explicit IssuerCaRefFilter(const vector<string>& issuer_ca_ref_list)
    : issuer_ca_ref_list_(issuer_ca_ref_list) {}
  bool operator()(const scoped_refptr<net::X509Certificate>& cert) const {
    // Find the certificate issuer for each certificate.
    // TODO(gspencer): this functionality should be available from
    // X509Certificate or CertDatabase.
    CERTCertificate* issuer_cert = CERT_FindCertIssuer(
        cert.get()->os_cert_handle(), PR_Now(), certUsageAnyCA);

    if (issuer_cert && issuer_cert->nickname) {
      // Separate the nickname stored in the certificate at the colon, since
      // NSS likes to store it as token:nickname.
      const char* delimiter =
          ::strchr(cert.get()->os_cert_handle()->nickname, ':');
      if (delimiter) {
        delimiter++;  // move past the colon.
        vector<string>::const_iterator pat_iter = issuer_ca_ref_list_.begin();
        while (pat_iter != issuer_ca_ref_list_.end()) {
          if (::strcmp(delimiter, pat_iter->c_str()) == 0)
            return false;
          ++pat_iter;
        }
      }
    }
    return true;
  }
 private:
  const vector<string>& issuer_ca_ref_list_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// IssuerSubjectPattern
IssuerSubjectPattern::IssuerSubjectPattern(const std::string& common_name,
                     const std::string& locality,
                     const std::string& organization,
                     const std::string& organizational_unit)
    : common_name_(common_name),
      locality_(locality),
      organization_(organization),
      organizational_unit_(organizational_unit) { }

IssuerSubjectPattern::IssuerSubjectPattern() {}

IssuerSubjectPattern::~IssuerSubjectPattern() {}

bool IssuerSubjectPattern::Matches(const net::CertPrincipal& principal) const {
  if (!common_name_.empty() && common_name_ != principal.common_name)
    return false;

  if (!locality_.empty() && locality_ != principal.locality_name)
    return false;

  if (!organization_.empty()) {
    if (find(principal.organization_names.begin(),
             principal.organization_names.end(), organization_) ==
        principal.organization_names.end()) {
      return false;
    }
  }

  if (!organizational_unit_.empty()) {
    if (find(principal.organization_unit_names.begin(),
             principal.organization_unit_names.end(),
             organizational_unit_) == principal.organization_unit_names.end()) {
      return false;
    }
  }

  return true;
}

bool IssuerSubjectPattern::Empty() const {
  return common_name_.empty() &&
         locality_.empty() &&
         organization_.empty() &&
         organizational_unit_.empty();
}

////////////////////////////////////////////////////////////////////////////////
// CertificatePattern

CertificatePattern::CertificatePattern() {}

CertificatePattern::~CertificatePattern() {}

bool CertificatePattern::Empty() const {
  return issuer_ca_ref_list_.empty() &&
         issuer_.Empty() &&
         subject_.Empty();
}

scoped_refptr<net::X509Certificate> CertificatePattern::GetMatch() const {
  typedef list<scoped_refptr<net::X509Certificate> > CertificateStlList;

  // Start with all the certs, and narrow it down from there.
  net::CertificateList all_certs;
  CertificateStlList matching_certs;
  net::CertDatabase cert_db;
  cert_db.ListCerts(&all_certs);

  if (all_certs.empty())
    return NULL;

  for (net::CertificateList::iterator iter = all_certs.begin();
       iter != all_certs.end(); ++iter) {
    matching_certs.push_back(*iter);
  }

  // Strip off any certs that don't have the right issuer and/or subject.
  if (!issuer_.Empty()) {
    matching_certs.remove_if(IssuerFilter(issuer_));
    if (matching_certs.empty())
      return NULL;
  }

  if (!subject_.Empty()) {
    matching_certs.remove_if(SubjectFilter(subject_));
    if (matching_certs.empty())
      return NULL;
  }

  if (!issuer_ca_ref_list_.empty()) {
    matching_certs.remove_if(IssuerCaRefFilter(issuer_ca_ref_list_));
    if (matching_certs.empty())
      return NULL;
  }

  // Eliminate any certs that don't have private keys associated with
  // them.  The CheckUserCert call in the filter is a little slow (because of
  // underlying PKCS11 calls), so we do this last to reduce the number of times
  // we have to call it.
  PrivateKeyFilter private_filter(&cert_db);
  matching_certs.remove_if(private_filter);

  if (matching_certs.empty())
    return NULL;

  // We now have a list of certificates that match the pattern we're
  // looking for.  Now we find the one with the latest start date.
  scoped_refptr<net::X509Certificate> latest(NULL);

  // Iterate over the rest looking for the one that was issued latest.
  for (CertificateStlList::iterator iter = matching_certs.begin();
       iter != matching_certs.end(); ++iter) {
    if (!latest.get() || (*iter)->valid_start() > latest->valid_start())
      latest = *iter;
  }

  return latest;
}

}  // namespace chromeos
