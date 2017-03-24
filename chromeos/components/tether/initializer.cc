// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/initializer.h"

namespace chromeos {

namespace tether {

// static
Initializer* Initializer::instance_ = nullptr;

void Initializer::Initialize(cryptauth::CryptAuthService* cryptauth_service) {
  if (instance_) {
    // TODO(khorimoto): Determine if a new instance should be created.
    instance_->cryptauth_service_ = cryptauth_service;
  } else {
    instance_ = new Initializer(cryptauth_service);
  }
}

Initializer::Initializer(cryptauth::CryptAuthService* cryptauth_service)
    : cryptauth_service_(cryptauth_service) {}

Initializer::~Initializer() {}

}  // namespace tether

}  // namespace chromeos
