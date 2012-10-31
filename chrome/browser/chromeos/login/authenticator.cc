// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/authenticator.h"

namespace chromeos {

class LoginStatusConsumer;

Authenticator::Authenticator(LoginStatusConsumer* consumer)
    : consumer_(consumer),
      authentication_profile_(NULL) {
}

Authenticator::~Authenticator() {}

void Authenticator::SetConsumer(LoginStatusConsumer* consumer) {
  consumer_ = consumer;
}

}  // namespace chromeos
