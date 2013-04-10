// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/certificate_pattern_matcher.h"

#include <cert.h>
#include <pk11pub.h>

#include <list>
#include <string>
#include <vector>

#include "chromeos/network/certificate_pattern.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"

namespace chromeos {

namespace {

// Returns true only if any fields set in this pattern match exactly with
// similar fields in the principal.  If organization_ or organizational_unit_
// are set, then at least one of the organizations or units in the principal
// must match.
bool CertPrincipalMatches(const IssuerSubjectPattern& pattern,
                          const net::CertPrincipal& principal) {
  if (!pattern.common_name().empty() &&
      pattern.common_name() != principal.common_name) {
    return false;
  }

  if (!pattern.locality().empty() &&
      pattern.locality() != principal.locality_name) {
    return false;
  }

  if (!pattern.organization().empty()) {
    if (std::find(principal.organization_names.begin(),
                  principal.organization_names.end(),
                  pattern.organization()) ==
        principal.organization_names.end()) {
      return false;
    }
  }

  if (!pattern.organizational_unit().empty()) {
    if (std::find(principal.organization_unit_names.begin(),
                  principal.organization_unit_names.end(),
                  pattern.organizational_unit()) ==
        principal.organization_unit_names.end()) {
      return false;
    }
  }

  return true;
}

// Functor to filter out non-matching issuers.
class IssuerFilter {
 public:
  explicit IssuerFilter(const IssuerSubjectPattern& issuer)
    : issuer_(issuer) {}
  bool operator()(const scoped_refptr<net::X509Certificate>& cert) const {
    return !CertPrincipalMatches(issuer_, cert.get()->issuer());
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
    return !CertPrincipalMatches(subject_, cert.get()->subject());
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
  explicit IssuerCaRefFilter(const std::vector<std::string>& issuer_ca_ref_list)
    : issuer_ca_ref_list_(issuer_ca_ref_list) {}
  bool operator()(const scoped_refptr<net::X509Certificate>& cert) const {
    // Find the certificate issuer for each certificate.
    // TODO(gspencer): this functionality should be available from
    // X509Certificate or NSSCertDatabase.
    CERTCertificate* issuer_cert = CERT_FindCertIssuer(
        cert.get()->os_cert_handle(), PR_Now(), certUsageAnyCA);

    if (issuer_cert && issuer_cert->nickname) {
      // Separate the nickname stored in the certificate at the colon, since
      // NSS likes to store it as token:nickname.
      const char* delimiter = ::strchr(issuer_cert->nickname, ':');
      if (delimiter) {
        delimiter++;  // move past the colon.
        std::vector<std::string>::const_iterator pat_iter =
            issuer_ca_ref_list_.begin();
        while (pat_iter != issuer_ca_ref_list_.end()) {
          if (*pat_iter == delimiter)
            return false;
          ++pat_iter;
        }
      }
    }
    return true;
  }
 private:
  const std::vector<std::string>& issuer_ca_ref_list_;
};

}  // namespace

scoped_refptr<net::X509Certificate> GetCertificateMatch(
    const CertificatePattern& pattern) {
  typedef std::list<scoped_refptr<net::X509Certificate> > CertificateStlList;

  // Start with all the certs, and narrow it down from there.
  net::CertificateList all_certs;
  CertificateStlList matching_certs;
  net::NSSCertDatabase::GetInstance()->ListCerts(&all_certs);

  if (all_certs.empty())
    return NULL;

  for (net::CertificateList::iterator iter = all_certs.begin();
       iter != all_certs.end(); ++iter) {
    matching_certs.push_back(*iter);
  }

  // Strip off any certs that don't have the right issuer and/or subject.
  if (!pattern.issuer().Empty()) {
    matching_certs.remove_if(IssuerFilter(pattern.issuer()));
    if (matching_certs.empty())
      return NULL;
  }

  if (!pattern.subject().Empty()) {
    matching_certs.remove_if(SubjectFilter(pattern.subject()));
    if (matching_certs.empty())
      return NULL;
  }

  if (!pattern.issuer_ca_ref_list().empty()) {
    matching_certs.remove_if(IssuerCaRefFilter(pattern.issuer_ca_ref_list()));
    if (matching_certs.empty())
      return NULL;
  }

  // Eliminate any certs that don't have private keys associated with
  // them.  The CheckUserCert call in the filter is a little slow (because of
  // underlying PKCS11 calls), so we do this last to reduce the number of times
  // we have to call it.
  PrivateKeyFilter private_filter(net::CertDatabase::GetInstance());
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
