// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/chromeos/language_options_util2.h"

namespace chromeos {
namespace options2 {

// See comments in .h.
Value* CreateValue(const char* in_value) {
  return Value::CreateStringValue(in_value);
}

Value* CreateValue(int in_value) {
  return Value::CreateIntegerValue(in_value);
}

}  // namespace options2
}  // namespace chromeos
