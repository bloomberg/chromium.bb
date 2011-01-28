// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/x509_openssl_util.h"

#include <algorithm>

#include "base/logging.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "net/base/x509_cert_types.h"

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

  base::StringPiece str_date(reinterpret_cast<const char*>(x509_time->data),
                             x509_time->length);

  CertDateFormat format = x509_time->type == V_ASN1_UTCTIME ?
      CERT_DATE_FORMAT_UTC_TIME : CERT_DATE_FORMAT_GENERALIZED_TIME;
  return ParseCertificateDate(str_date, format, time);
}

// TODO(joth): Investigate if we can upstream this into the OpenSSL library,
// to avoid duplicating this logic across projects.
bool VerifyHostname(const std::string& hostname,
                    const std::vector<std::string>& cert_names) {
  DCHECK(!hostname.empty());

  // Simple host name validation. A valid domain name must only contain
  // alpha, digits, hyphens, and dots. An IP address may have digits and dots,
  // and also square braces and colons for IPv6 addresses.
  std::string reference_name;
  reference_name.reserve(hostname.length());

  bool found_alpha = false;
  bool found_ip6_chars = false;
  bool found_hyphen = false;
  int dot_count = 0;

  size_t first_dot_index = std::string::npos;
  for (std::string::const_iterator it = hostname.begin();
       it != hostname.end(); ++it) {
    char c = *it;
    if (IsAsciiAlpha(c)) {
      found_alpha = true;
      c = base::ToLowerASCII(c);
    } else if (c == '.') {
      ++dot_count;
      if (first_dot_index == std::string::npos)
        first_dot_index = reference_name.length();
    } else if (c == ':') {
      found_ip6_chars = true;
    } else if (c == '-') {
      found_hyphen = true;
    } else if (!IsAsciiDigit(c)) {
      LOG(WARNING) << "Invalid char " << c << " in hostname " << hostname;
      return false;
    }
    reference_name.push_back(c);
  }
  DCHECK(!reference_name.empty());

  if (found_ip6_chars || !found_alpha) {
    // For now we just do simple localhost IP address support, primarily as
    // it's needed by the test server. TODO(joth): Replace this with full IP
    // address support. See http://crbug.com/62973
    if (hostname == "127.0.0.1" &&
        std::find(cert_names.begin(), cert_names.end(), hostname) !=
            cert_names.end()) {
      DVLOG(1) << "Allowing localhost IP certificate: " << hostname;
      return true;
    }
    NOTIMPLEMENTED() << hostname;  // See comment above.
    return false;
  }

  // |wildcard_domain| is the remainder of |host| after the leading host
  // component is stripped off, but includes the leading dot e.g.
  // "www.f.com" -> ".f.com".
  // If there is no meaningful domain part to |host| (e.g. it is an IP address
  // or contains no dots) then |wildcard_domain| will be empty.
  // We required at least 3 components (i.e. 2 dots) as a basic protection
  // against too-broad wild-carding.
  base::StringPiece wildcard_domain;
  if (found_alpha && !found_ip6_chars && dot_count >= 2) {
    DCHECK(first_dot_index != std::string::npos);
    wildcard_domain = reference_name;
    wildcard_domain.remove_prefix(first_dot_index);
    DCHECK(wildcard_domain.starts_with("."));
  }

  for (std::vector<std::string>::const_iterator it = cert_names.begin();
       it != cert_names.end(); ++it) {
    // Catch badly corrupt cert names up front.
    if (it->empty() || it->find('\0') != std::string::npos) {
      LOG(WARNING) << "Bad name in cert: " << *it;
      continue;
    }
    const std::string cert_name_string(StringToLowerASCII(*it));
    base::StringPiece cert_match(cert_name_string);

    // Remove trailing dot, if any.
    if (cert_match.ends_with("."))
      cert_match.remove_suffix(1);

    // The hostname must be at least as long as the cert name it is matching,
    // as we require the wildcard (if present) to match at least one character.
    if (cert_match.length() > reference_name.length())
      continue;

    if (cert_match == reference_name)
      return true;

    // Next see if this cert name starts with a wildcard, so long as the
    // hostname we're matching against has a valid 'domain' part to match.
    // Note the "-10" version of draft-saintandre-tls-server-id-check allows
    // the wildcard to appear anywhere in the leftmost label, rather than
    // requiring it to be the only character. See also http://crbug.com/60719
    if (wildcard_domain.empty() || !cert_match.starts_with("*"))
      continue;

    // Erase the * but not the . from the domain, as we need to include the dot
    // in the comparison.
    cert_match.remove_prefix(1);

    // Do character by character comparison on the remainder to see
    // if we have a wildcard match. This intentionally does no special handling
    // for any other wildcard characters in |domain|; alternatively it could
    // detect these and skip those candidate cert names.
    if (cert_match == wildcard_domain)
      return true;
  }
  DVLOG(1) << "Could not find any match for " << hostname
           << " (canonicalized as " << reference_name
           << ") in cert names " << JoinString(cert_names, '|');
  return false;
}

}  // namespace x509_openssl_util

}  // namespace net
