// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/dom_options_util.h"

#include "base/string_util.h"

namespace dom_options_util {

string16 StripColon(const string16& str) {
  const string16::value_type kColon[] = {':',0};
  string16 result;
  RemoveChars(str, kColon, &result);
  return result;
}

}  // namespace dom_options_util
