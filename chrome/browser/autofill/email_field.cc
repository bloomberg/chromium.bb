// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/email_field.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_scanner.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kEmailRe[] =
    "e.?mail"
    // ja-JP
    "|\xe3\x83\xa1\xe3\x83\xbc\xe3\x83\xab\xe3\x82\xa2\xe3\x83\x89\xe3\x83\xac"
        "\xe3\x82\xb9"
    // ru
    "|\xd0\xad\xd0\xbb\xd0\xb5\xd0\xba\xd1\x82\xd1\x80\xd0\xbe\xd0\xbd\xd0\xbd"
        "\xd0\xbe\xd0\xb9.?\xd0\x9f\xd0\xbe\xd1\x87\xd1\x82\xd1\x8b"
    // zh-CN
    "|\xe9\x82\xae\xe4\xbb\xb6|\xe9\x82\xae\xe7\xae\xb1"
    // zh-TW
    "|\xe9\x9b\xbb\xe9\x83\xb5\xe5\x9c\xb0\xe5\x9d\x80"
    // ko-KR
    "|(\xec\x9d\xb4\xeb\xa9\x94\xec\x9d\xbc|\xec\xa0\x84\xec\x9e\x90.?\xec\x9a"
        "\xb0\xed\x8e\xb8|[Ee]-?mail)(.?\xec\xa3\xbc\xec\x86\x8c)?";

}  // namespace

// static
FormField* EmailField::Parse(AutofillScanner* scanner) {
  const AutofillField* field;
  if (ParseFieldSpecifics(scanner, UTF8ToUTF16(kEmailRe),
                          MATCH_DEFAULT | MATCH_EMAIL, &field)) {
    return new EmailField(field);
  }

  return NULL;
}

EmailField::EmailField(const AutofillField* field) : field_(field) {
}

bool EmailField::ClassifyField(FieldTypeMap* map) const {
  return AddClassification(field_, EMAIL_ADDRESS, map);
}
