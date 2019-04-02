// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/label_formatter_test_utils.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

base::string16 FormatExpectedLabel(base::StringPiece label_part1,
                                   base::StringPiece label_part2) {
  return l10n_util::GetStringFUTF16(IDS_AUTOFILL_SUGGESTION_LABEL,
                                    base::UTF8ToUTF16(label_part1),
                                    base::UTF8ToUTF16(label_part2));
}

}  // namespace autofill