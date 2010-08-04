// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/login_library.h"

#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"

namespace chromeos {

class LoginLibraryImpl : public LoginLibrary {
 public:
  LoginLibraryImpl() {}
  virtual ~LoginLibraryImpl() {}

  bool EmitLoginPromptReady() {
    return chromeos::EmitLoginPromptReady();
  }

  bool StartSession(const std::string& user_email,
                    const std::string& unique_id /* unused */) {
    // only pass unique_id through once we use it for something.
    return chromeos::StartSession(user_email.c_str(), "");
  }

  bool StopSession(const std::string& unique_id /* unused */) {
    // only pass unique_id through once we use it for something.
    return chromeos::StopSession("");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginLibraryImpl);
};

class LoginLibraryStubImpl : public LoginLibrary {
 public:
  LoginLibraryStubImpl() {}
  virtual ~LoginLibraryStubImpl() {}

  bool EmitLoginPromptReady() { return true; }
  bool StartSession(const std::string& user_email,
                    const std::string& unique_id /* unused */) { return true; }
  bool StopSession(const std::string& unique_id /* unused */) { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginLibraryStubImpl);
};

// static
LoginLibrary* LoginLibrary::GetImpl(bool stub) {
  if (stub)
    return new LoginLibraryStubImpl();
  else
    return new LoginLibraryImpl();
}

}  // namespace chromeos
