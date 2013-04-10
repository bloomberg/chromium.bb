// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/certificate_pattern.h"

#include "base/logging.h"
#include "base/values.h"

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

base::ListValue* CreateListFromStrings(
    const std::vector<std::string>& strings) {
  base::ListValue* new_list = new base::ListValue;
  for (std::vector<std::string>::const_iterator iter = strings.begin();
       iter != strings.end(); ++iter) {
    new_list->Append(new StringValue(*iter));
  }
  return new_list;
}

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

base::DictionaryValue* IssuerSubjectPattern::CreateAsDictionary() const {
  base::DictionaryValue* dict = new base::DictionaryValue;
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

bool IssuerSubjectPattern::CopyFromDictionary(
    const base::DictionaryValue& dict) {
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

base::DictionaryValue* CertificatePattern::CreateAsDictionary() const {
  base::DictionaryValue* dict = new base::DictionaryValue;

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

bool CertificatePattern::CopyFromDictionary(const base::DictionaryValue &dict) {
  const base::DictionaryValue* child_dict = NULL;
  const base::ListValue* child_list = NULL;
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
