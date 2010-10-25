// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_openssl_util.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/time.h"

namespace net {

namespace x509_openssl_util {

bool ParsePrincipalKeyAndValueByIndex(X509_NAME* name,
                                      int index,
                                      std::string* key,
                                      std::string* value) {
  X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, index);
  if (!entry)
    return false;

  if (key) {
    ASN1_OBJECT* object = X509_NAME_ENTRY_get_object(entry);
    key->assign(OBJ_nid2sn(OBJ_obj2nid(object)));
  }

  ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
  if (!data)
    return false;

  unsigned char* buf = NULL;
  int len = ASN1_STRING_to_UTF8(&buf, data);
  if (len <= 0)
    return false;

  value->assign(reinterpret_cast<const char*>(buf), len);
  OPENSSL_free(buf);
  return true;
}

bool ParsePrincipalValueByIndex(X509_NAME* name,
                                int index,
                                std::string* value) {
  return ParsePrincipalKeyAndValueByIndex(name, index, NULL, value);
}

bool ParsePrincipalValueByNID(X509_NAME* name, int nid, std::string* value) {
  int index = X509_NAME_get_index_by_NID(name, nid, -1);
  if (index < 0)
    return false;

  return ParsePrincipalValueByIndex(name, index, value);
}

bool ParseDate(ASN1_TIME* x509_time, base::Time* time) {
  if (!x509_time ||
      (x509_time->type != V_ASN1_UTCTIME &&
       x509_time->type != V_ASN1_GENERALIZEDTIME))
    return false;

  std::string str_date(reinterpret_cast<char*>(x509_time->data),
                       x509_time->length);
  // UTCTime: YYMMDDHHMMSSZ
  // GeneralizedTime: YYYYMMDDHHMMSSZ
  size_t year_length = x509_time->type == V_ASN1_UTCTIME ? 2 : 4;
  size_t fields_offset = x509_time->type == V_ASN1_UTCTIME ? 0 : 2;

  if (str_date.length() < 11 + year_length)
    return false;

  base::Time::Exploded exploded = {0};
  bool valid = base::StringToInt(str_date.begin(),
                                 str_date.begin() + year_length,
                                 &exploded.year);
  if (valid && year_length == 2)
    exploded.year += exploded.year < 50 ? 2000 : 1900;

  valid &= base::StringToInt(str_date.begin() + fields_offset + 2,
                             str_date.begin() + fields_offset + 4,
                             &exploded.month);
  valid &= base::StringToInt(str_date.begin() + fields_offset + 4,
                             str_date.begin() + fields_offset + 6,
                             &exploded.day_of_month);
  valid &= base::StringToInt(str_date.begin() + fields_offset + 6,
                             str_date.begin() + fields_offset + 8,
                             &exploded.hour);
  valid &= base::StringToInt(str_date.begin() + fields_offset + 8,
                             str_date.begin() + fields_offset + 10,
                             &exploded.minute);
  valid &= base::StringToInt(str_date.begin() + fields_offset + 10,
                             str_date.begin() + fields_offset + 12,
                             &exploded.second);

  if (!valid)
    return false;

  *time = base::Time::FromUTCExploded(exploded);
  return valid;
}

} // namespace x509_openssl_util

} // namespace net
