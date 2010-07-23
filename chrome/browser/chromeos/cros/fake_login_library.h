// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_FAKE_LOGIN_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_FAKE_LOGIN_LIBRARY_H_

#include <string>

#include "chrome/browser/chromeos/cros/login_library.h"

namespace chromeos {

class FakeLoginLibrary : public LoginLibrary {
 public:
  FakeLoginLibrary() {}
  virtual ~FakeLoginLibrary() {}
  virtual bool EmitLoginPromptReady();
  virtual bool StartSession(const std::string& user_email,
                            const std::string& unique_id);
  virtual bool StopSession(const std::string& unique_id);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_FAKE_LOGIN_LIBRARY_H_
