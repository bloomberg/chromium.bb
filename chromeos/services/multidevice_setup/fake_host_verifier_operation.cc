// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/fake_host_verifier_operation.h"

namespace chromeos {

namespace multidevice_setup {

FakeHostVerifierOperation::FakeHostVerifierOperation(Delegate* delegate)
    : HostVerifierOperation(delegate) {}

FakeHostVerifierOperation::~FakeHostVerifierOperation() = default;

void FakeHostVerifierOperation::PerformCancelOperation() {}

FakeHostVerifierOperationDelegate::FakeHostVerifierOperationDelegate() =
    default;

FakeHostVerifierOperationDelegate::~FakeHostVerifierOperationDelegate() =
    default;

void FakeHostVerifierOperationDelegate::OnOperationFinished(
    HostVerifierOperation::Result result) {
  DCHECK(!result_);
  result_ = result;
}

}  // namespace multidevice_setup

}  // namespace chromeos
