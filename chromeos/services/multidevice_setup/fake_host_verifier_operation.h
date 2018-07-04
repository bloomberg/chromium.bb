// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_VERIFIER_OPERATION_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_VERIFIER_OPERATION_H_

#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/services/multidevice_setup/host_verifier_operation.h"

namespace chromeos {

namespace multidevice_setup {

// Test HostVerifierOperation implementation.
class FakeHostVerifierOperation : public HostVerifierOperation {
 public:
  FakeHostVerifierOperation(Delegate* delegate);
  ~FakeHostVerifierOperation() override;

  using HostVerifierOperation::NotifyOperationFinished;

 private:
  // HostVerifierOperation:
  void PerformCancelOperation() override;

  DISALLOW_COPY_AND_ASSIGN(FakeHostVerifierOperation);
};

// Test HostVerifierOperation::Delegate implementation.
class FakeHostVerifierOperationDelegate
    : public HostVerifierOperation::Delegate {
 public:
  FakeHostVerifierOperationDelegate();
  ~FakeHostVerifierOperationDelegate() override;

  const base::Optional<HostVerifierOperation::Result>& result() const {
    return result_;
  }

 private:
  // HostVerifierOperation::Delegate:
  void OnOperationFinished(HostVerifierOperation::Result result) override;

  base::Optional<HostVerifierOperation::Result> result_;

  DISALLOW_COPY_AND_ASSIGN(FakeHostVerifierOperationDelegate);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_FAKE_HOST_VERIFIER_OPERATION_H_
