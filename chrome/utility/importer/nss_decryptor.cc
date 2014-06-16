// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/nss_decryptor.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "sql/connection.h"
#include "sql/statement.h"

#if defined(USE_NSS)
#include <pk11pub.h>
#include <pk11sdr.h>
#endif  // defined(USE_NSS)

// This method is based on some Firefox code in
//   security/manager/ssl/src/nsSDR.cpp
// The license block is:

/* ***** BEGIN LICENSE BLOCK *****
* Version: MPL 1.1/GPL 2.0/LGPL 2.1
*
* The contents of this file are subject to the Mozilla Public License Version
* 1.1 (the "License"); you may not use this file except in compliance with
* the License. You may obtain a copy of the License at
* http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* The Original Code is the Netscape security libraries.
*
* The Initial Developer of the Original Code is
* Netscape Communications Corporation.
* Portions created by the Initial Developer are Copyright (C) 1994-2000
* the Initial Developer. All Rights Reserved.
*
* Contributor(s):
*
* Alternatively, the contents of this file may be used under the terms of
* either the GNU General Public License Version 2 or later (the "GPL"), or
* the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
* in which case the provisions of the GPL or the LGPL are applicable instead
* of those above. If you wish to allow use of your version of this file only
* under the terms of either the GPL or the LGPL, and not to allow others to
* use your version of this file under the terms of the MPL, indicate your
* decision by deleting the provisions above and replace them with the notice
* and other provisions required by the GPL or the LGPL. If you do not delete
* the provisions above, a recipient may use your version of this file under
* the terms of any one of the MPL, the GPL or the LGPL.
*
* ***** END LICENSE BLOCK ***** */

base::string16 NSSDecryptor::Decrypt(const std::string& crypt) const {
  // Do nothing if NSS is not loaded.
  if (!is_nss_initialized_)
    return base::string16();

  // The old style password is encoded in base64. They are identified
  // by a leading '~'. Otherwise, we should decrypt the text.
  std::string plain;
  if (crypt[0] != '~') {
    std::string decoded_data;
    base::Base64Decode(crypt, &decoded_data);
    PK11SlotInfo* slot = GetKeySlotForDB();
    SECStatus result = PK11_Authenticate(slot, PR_TRUE, NULL);
    if (result != SECSuccess) {
      FreeSlot(slot);
      return base::string16();
    }

    SECItem request;
    request.data = reinterpret_cast<unsigned char*>(
        const_cast<char*>(decoded_data.data()));
    request.len = static_cast<unsigned int>(decoded_data.size());
    SECItem reply;
    reply.data = NULL;
    reply.len = 0;
#if defined(USE_NSS)
    result = PK11SDR_DecryptWithSlot(slot, &request, &reply, NULL);
#else
    result = PK11SDR_Decrypt(&request, &reply, NULL);
#endif  // defined(USE_NSS)
    if (result == SECSuccess)
      plain.assign(reinterpret_cast<char*>(reply.data), reply.len);

    SECITEM_FreeItem(&reply, PR_FALSE);
    FreeSlot(slot);
  } else {
    // Deletes the leading '~' before decoding.
    base::Base64Decode(crypt.substr(1), &plain);
  }

  return base::UTF8ToUTF16(plain);
}

// There are three versions of password files. They store saved user
// names and passwords.
// References:
// http://kb.mozillazine.org/Signons.txt
// http://kb.mozillazine.org/Signons2.txt
// http://kb.mozillazine.org/Signons3.txt
void NSSDecryptor::ParseSignons(
    const std::string& content,
    std::vector<autofill::PasswordForm>* forms) {
  forms->clear();

  // Splits the file content into lines.
  std::vector<std::string> lines;
  base::SplitString(content, '\n', &lines);

  // The first line is the file version. We skip the unknown versions.
  if (lines.empty())
    return;
  int version;
  if (lines[0] == "#2c")
    version = 1;
  else if (lines[0] == "#2d")
    version = 2;
  else if (lines[0] == "#2e")
    version = 3;
  else
    return;

  GURL::Replacements rep;
  rep.ClearQuery();
  rep.ClearRef();
  rep.ClearUsername();
  rep.ClearPassword();

  // Reads never-saved list. Domains are stored one per line.
  size_t i;
  for (i = 1; i < lines.size() && lines[i].compare(".") != 0; ++i) {
    autofill::PasswordForm form;
    form.origin = GURL(lines[i]).ReplaceComponents(rep);
    form.signon_realm = form.origin.GetOrigin().spec();
    form.blacklisted_by_user = true;
    forms->push_back(form);
  }
  ++i;

  // Reads saved passwords. The information is stored in blocks
  // seperated by lines that only contain a dot. We find a block
  // by the seperator and parse them one by one.
  while (i < lines.size()) {
    size_t begin = i;
    size_t end = i + 1;
    while (end < lines.size() && lines[end].compare(".") != 0)
      ++end;
    i = end + 1;

    // A block has at least five lines.
    if (end - begin < 5)
      continue;

    autofill::PasswordForm form;

    // The first line is the site URL.
    // For HTTP authentication logins, the URL may contain http realm,
    // which will be in bracket:
    //   sitename:8080 (realm)
    GURL url;
    std::string realm;
    const char kRealmBracketBegin[] = " (";
    const char kRealmBracketEnd[] = ")";
    if (lines[begin].find(kRealmBracketBegin) != std::string::npos) {
      // In this case, the scheme may not exsit. We assume that the
      // scheme is HTTP.
      if (lines[begin].find("://") == std::string::npos)
        lines[begin] = "http://" + lines[begin];

      size_t start = lines[begin].find(kRealmBracketBegin);
      url = GURL(lines[begin].substr(0, start));

      start += std::string(kRealmBracketBegin).size();
      size_t end = lines[begin].rfind(kRealmBracketEnd);
      realm = lines[begin].substr(start, end - start);
    } else {
      // Don't have http realm. It is the URL that the following passwords
      // belong to.
      url = GURL(lines[begin]);
    }
    // Skips this block if the URL is not valid.
    if (!url.is_valid())
      continue;
    form.origin = url.ReplaceComponents(rep);
    form.signon_realm = form.origin.GetOrigin().spec();
    if (!realm.empty())
      form.signon_realm += realm;
    form.ssl_valid = form.origin.SchemeIsSecure();
    ++begin;

    // There may be multiple username/password pairs for this site.
    // In this case, they are saved in one block without a seperated
    // line (contains a dot).
    while (begin + 4 < end) {
      // The user name.
      form.username_element = base::UTF8ToUTF16(lines[begin++]);
      form.username_value = Decrypt(lines[begin++]);
      // The element name has a leading '*'.
      if (lines[begin].at(0) == '*') {
        form.password_element = base::UTF8ToUTF16(lines[begin++].substr(1));
        form.password_value = Decrypt(lines[begin++]);
      } else {
        // Maybe the file is bad, we skip to next block.
        break;
      }
      // The action attribute from the form element. This line exists
      // in versin 2 or above.
      if (version >= 2) {
        if (begin < end)
          form.action = GURL(lines[begin]).ReplaceComponents(rep);
        ++begin;
      }
      // Version 3 has an extra line for further use.
      if (version == 3) {
        ++begin;
      }

      forms->push_back(form);
    }
  }
}

bool NSSDecryptor::ReadAndParseSignons(const base::FilePath& sqlite_file,
    std::vector<autofill::PasswordForm>* forms) {
  sql::Connection db;
  if (!db.Open(sqlite_file))
    return false;

  const char* query = "SELECT hostname FROM moz_disabledHosts";
  sql::Statement s(db.GetUniqueStatement(query));
  if (!s.is_valid())
    return false;

  GURL::Replacements rep;
  rep.ClearQuery();
  rep.ClearRef();
  rep.ClearUsername();
  rep.ClearPassword();
  // Read domains for which passwords are never saved.
  while (s.Step()) {
    autofill::PasswordForm form;
    form.origin = GURL(s.ColumnString(0)).ReplaceComponents(rep);
    form.signon_realm = form.origin.GetOrigin().spec();
    form.blacklisted_by_user = true;
    forms->push_back(form);
  }

  const char* query2 = "SELECT hostname, httpRealm, formSubmitURL, "
                       "usernameField, passwordField, encryptedUsername, "
                       "encryptedPassword FROM moz_logins";

  sql::Statement s2(db.GetUniqueStatement(query2));
  if (!s2.is_valid())
    return false;

  while (s2.Step()) {
    GURL url;
    std::string realm(s2.ColumnString(1));
    if (!realm.empty()) {
      // In this case, the scheme may not exsit. Assume HTTP.
      std::string host(s2.ColumnString(0));
      if (host.find("://") == std::string::npos)
        host = "http://" + host;
      url = GURL(host);
    } else {
      url = GURL(s2.ColumnString(0));
    }
    // Skip this row if the URL is not valid.
    if (!url.is_valid())
      continue;

    autofill::PasswordForm form;
    form.origin = url.ReplaceComponents(rep);
    form.signon_realm = form.origin.GetOrigin().spec();
    if (!realm.empty()) {
      form.signon_realm += realm;
      // Non-empty realm indicates that it's not html form authentication entry.
      // Extracted data doesn't allow us to distinguish basic_auth entry from
      // digest_auth entry, so let's assume basic_auth.
      form.scheme = autofill::PasswordForm::SCHEME_BASIC;
    }
    form.ssl_valid = form.origin.SchemeIsSecure();
    // The user name, password and action.
    form.username_element = s2.ColumnString16(3);
    form.username_value = Decrypt(s2.ColumnString(5));
    form.password_element = s2.ColumnString16(4);
    form.password_value = Decrypt(s2.ColumnString(6));
    form.action = GURL(s2.ColumnString(2)).ReplaceComponents(rep);
    forms->push_back(form);
  }
  return true;
}
