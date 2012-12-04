// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/language_options_util.h"

namespace chromeos {
namespace options {

// See comments in .h.
base::Value* CreateValue(const char* in_value) {
  return new base::StringValue(in_value);
}

base::Value* CreateValue(int in_value) {
  return new base::FundamentalValue(in_value);
}

}  // namespace options
}  // namespace chromeos
