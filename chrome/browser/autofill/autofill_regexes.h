// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_REGEXES_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_REGEXES_H_
#pragma once

#include "base/string16.h"

// Parsing utilities.
namespace autofill {

// Case-insensitive regular expression matching.
// Returns true if |pattern| is found in |input|.
bool MatchesPattern(const string16& input, const string16& pattern);

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_REGEXES_H_
