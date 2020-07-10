// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/authenticator.h"

namespace chromeos {

class AuthStatusConsumer;

Authenticator::Authenticator(AuthStatusConsumer* consumer)
    : consumer_(consumer), authentication_context_(NULL) {
}

Authenticator::~Authenticator() = default;

void Authenticator::SetConsumer(AuthStatusConsumer* consumer) {
  consumer_ = consumer;
}

}  // namespace chromeos
