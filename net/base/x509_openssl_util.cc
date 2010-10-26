// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_openssl_util.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/time.h"

namespace net {

namespace x509_openssl_util {

namespace {

// Helper for ParseDate. |*field| must contain at least |field_len| characters.
// |*field| will be advanced by |field_len| on exit. |*ok| is set to false if
// there is an error in parsing the number, but left untouched otherwise.
// Returns the parsed integer.
int ParseIntAndAdvance(const char** field, size_t field_len, bool* ok) {
  int result = 0;
  *ok &= base::StringToInt(*field, *field + field_len, &result);
  *field += field_len;
  return result;
}

}  // namespace

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

  base::StringPiece str_date(reinterpret_cast<const char*>(x509_time->data),
                             x509_time->length);
  // UTCTime: YYMMDDHHMMSSZ
  // GeneralizedTime: YYYYMMDDHHMMSSZ
  size_t year_length = x509_time->type == V_ASN1_UTCTIME ? 2 : 4;

  if (str_date.length() < 11 + year_length)
    return false;

  const char* field = str_date.data();
  bool valid = true;
  base::Time::Exploded exploded = {0};

  exploded.year =         ParseIntAndAdvance(&field, year_length, &valid);
  exploded.month =        ParseIntAndAdvance(&field, 2, &valid);
  exploded.day_of_month = ParseIntAndAdvance(&field, 2, &valid);
  exploded.hour =         ParseIntAndAdvance(&field, 2, &valid);
  exploded.minute =       ParseIntAndAdvance(&field, 2, &valid);
  exploded.second =       ParseIntAndAdvance(&field, 2, &valid);
  if (valid && year_length == 2)
    exploded.year += exploded.year < 50 ? 2000 : 1900;

  valid &= exploded.HasValidValues();

  if (!valid) {
    NOTREACHED() << "can't parse x509 date " << str_date;
    return false;
  }

  *time = base::Time::FromUTCExploded(exploded);
  return true;
}

} // namespace x509_openssl_util

} // namespace net
