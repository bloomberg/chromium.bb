// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_TEST_UTILS_H_

#include "base/strings/string16.h"
#include "base/strings/string_piece.h"

namespace autofill {

// Returns the the string to display to users according to the localized
// string IDS_AUTOFILL_SUGGESTION_LABEL, e.g. label_part1 • label_part2.
base::string16 FormatExpectedLabel(base::StringPiece label_part1,
                                   base::StringPiece label_part2);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LABEL_FORMATTER_TEST_UTILS_H_
