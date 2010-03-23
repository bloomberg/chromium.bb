// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/login_library.h"

#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"

namespace chromeos {

bool LoginLibraryImpl::EmitLoginPromptReady() {
  return chromeos::EmitLoginPromptReady();
}

bool LoginLibraryImpl::StartSession(const std::string& user_email,
                                const std::string& unique_id /* unused */) {
  // only pass unique_id through once we use it for something.
  return chromeos::StartSession(user_email.c_str(), "");
}

bool LoginLibraryImpl::StopSession(const std::string& unique_id /* unused */) {
  // only pass unique_id through once we use it for something.
  return chromeos::StopSession("");
}

}  // namespace chromeos
