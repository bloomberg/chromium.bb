// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/fake_login_library.h"

namespace chromeos {

bool FakeLoginLibrary::EmitLoginPromptReady() {
  return true;
}

bool FakeLoginLibrary::StartSession(const std::string& user_email,
                                    const std::string& unique_id) {
  return true;
}

bool FakeLoginLibrary::StopSession(const std::string& unique_id) {
  return true;
}

}  // namespace chromeos
