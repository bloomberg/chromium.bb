// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/fake_library_loader.h"

namespace chromeos {

bool FakeLibraryLoader::Load(std::string* load_error_string) {
  return true;
}

}   // namespace chromeos
