// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_INITIALIZER_H_
#define CHROMEOS_COMPONENTS_TETHER_INITIALIZER_H_

#include "base/macros.h"

namespace cryptauth {
class CryptAuthService;
}

namespace chromeos {

namespace tether {

// Initializes the Tether Chrome OS component.
// TODO(khorimoto): Implement.
class Initializer {
 public:
  static void Initialize(cryptauth::CryptAuthService* cryptauth_service);

 private:
  static Initializer* instance_;

  explicit Initializer(cryptauth::CryptAuthService* cryptauth_service);
  ~Initializer();

  cryptauth::CryptAuthService* cryptauth_service_;

  DISALLOW_COPY_AND_ASSIGN(Initializer);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_INITIALIZER_H_
