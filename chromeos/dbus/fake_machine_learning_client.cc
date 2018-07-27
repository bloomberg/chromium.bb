// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_machine_learning_client.h"

#include "base/callback.h"

namespace chromeos {

FakeMachineLearningClient::FakeMachineLearningClient() {}

void FakeMachineLearningClient::Init(dbus::Bus* const bus) {}

void FakeMachineLearningClient::BootstrapMojoConnection(
    base::ScopedFD fd,
    base::OnceCallback<void(bool success)> result_callback) {
  const bool success = false;
  std::move(result_callback).Run(success);
}

}  // namespace chromeos
