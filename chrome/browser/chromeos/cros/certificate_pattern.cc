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

#include "base/logging.h"
#include "base/values.h"
#include "net/base/cert_database.h"
#include "net/base/net_errors.h"
#include "net/base/x509_cert_types.h"
#include "net/base/x509_certificate.h"

// To shorten some of those long lines below.
using base::DictionaryValue;
using base::ListValue;
using std::find;
using std::list;
using std::string;
using std::vector;

namespace chromeos {

namespace {

// Keys for converting classes below to/from dictionaries.
const char kCommonNameKey[] = "CommonName";
const char kLocalityKey[] = "Locality";
const char kOrganizationKey[] = "Organization";
const char kOrganizationalUnitKey[] = "OrganizationalUnit";
const char kIssuerCaRefKey[] = "IssuerCARef";
const char kIssuerKey[] = "Issuer";
const char kSubjectKey[] = "Subject";
const char kEnrollmentUriKey[] = "EnrollmentURI";

bool GetAsListOfStrings(const base::Value& value,
                        std::vector<std::string>* result) {
  const base::ListValue* list = NULL;
  if (!value.GetAsList(&list))
    return false;
  result->clear();
  result->reserve(list->GetSize());
  for (size_t i = 0; i < list->GetSize(); i++) {
    std::string item;
    if (!list->GetString(i, &item))
      return false;
    result->push_back(item);
  }
  return true;
}

ListValue* CreateListFromStrings(const vector<string>& strings) {
  ListValue* new_list = new ListValue;
  for (vector<string>::const_iterator iter = strings.begin();
       iter != strings.end(); ++iter) {
    new_list->Append(new StringValue(*iter));
  }
  return new_list;
}

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
      const char* delimiter = ::strchr(issuer_cert->nickname, ':');
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

void IssuerSubjectPattern::Clear() {
  common_name_.clear();
  locality_.clear();
  organization_.clear();
  organizational_unit_.clear();
}

DictionaryValue* IssuerSubjectPattern::CreateAsDictionary() const {
  DictionaryValue* dict = new DictionaryValue;
  if (!common_name_.empty())
    dict->SetString(kCommonNameKey, common_name_);
  if (!locality_.empty())
    dict->SetString(kLocalityKey, locality_);
  if (!organization_.empty())
    dict->SetString(kOrganizationKey, organization_);
  if (!organizational_unit_.empty())
    dict->SetString(kOrganizationalUnitKey, organizational_unit_);
  return dict;
}

bool IssuerSubjectPattern::CopyFromDictionary(const DictionaryValue& dict) {
  Clear();
  dict.GetString(kCommonNameKey, &common_name_);
  dict.GetString(kLocalityKey, &locality_);
  dict.GetString(kOrganizationKey, &organization_);
  dict.GetString(kOrganizationalUnitKey, &organizational_unit_);
  // If the dictionary wasn't empty, but we are, or vice versa, then something
  // went wrong.
  DCHECK(dict.empty() == Empty());
  if (dict.empty() != Empty())
    return false;
  return true;
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

void CertificatePattern::Clear() {
  issuer_ca_ref_list_.clear();
  issuer_.Clear();
  subject_.Clear();
  enrollment_uri_list_.clear();
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

DictionaryValue* CertificatePattern::CreateAsDictionary() const {
  DictionaryValue* dict = new base::DictionaryValue;

  if (!issuer_ca_ref_list_.empty())
    dict->Set(kIssuerCaRefKey, CreateListFromStrings(issuer_ca_ref_list_));

  if (!issuer_.Empty())
    dict->Set(kIssuerKey, issuer_.CreateAsDictionary());

  if (!subject_.Empty())
    dict->Set(kSubjectKey, subject_.CreateAsDictionary());

  if (!enrollment_uri_list_.empty())
    dict->Set(kEnrollmentUriKey, CreateListFromStrings(enrollment_uri_list_));
  return dict;
}

bool CertificatePattern::CopyFromDictionary(const DictionaryValue &dict) {
  DictionaryValue* child_dict = NULL;
  ListValue* child_list = NULL;
  Clear();

  // All of these are optional.
  if (dict.GetList(kIssuerCaRefKey, &child_list) && child_list) {
    if (!GetAsListOfStrings(*child_list, &issuer_ca_ref_list_))
      return false;
  }
  if (dict.GetDictionary(kIssuerKey, &child_dict) && child_dict) {
    if (!issuer_.CopyFromDictionary(*child_dict))
      return false;
  }
  child_dict = NULL;
  if (dict.GetDictionary(kSubjectKey, &child_dict) && child_dict) {
    if (!subject_.CopyFromDictionary(*child_dict))
      return false;
  }
  child_list = NULL;
  if (dict.GetList(kEnrollmentUriKey, &child_list) && child_list) {
    if (!GetAsListOfStrings(*child_list, &enrollment_uri_list_))
      return false;
  }

  // If we didn't copy anything from the dictionary, then it had better be
  // empty.
  DCHECK(dict.empty() == Empty());
  if (dict.empty() != Empty())
    return false;

  return true;
}

}  // namespace chromeos
