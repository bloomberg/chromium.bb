// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_IMPL_H_
#define CHROMEOS_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_IMPL_H_

#include "base/macros.h"
#include "chromeos/services/cellular_setup/cellular_setup_base.h"

namespace chromeos {

namespace cellular_setup {

// Concrete mojom::CellularSetup implementation.
class CellularSetupImpl : public CellularSetupBase {
 public:
  class Factory {
   public:
    static std::unique_ptr<CellularSetupBase> Create();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<CellularSetupBase> BuildInstance() = 0;
  };

  ~CellularSetupImpl() override;

 private:
  CellularSetupImpl();

  // mojom::CellularSetup:
  void StartActivation(mojom::ActivationDelegatePtr delegate,
                       StartActivationCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(CellularSetupImpl);
};

}  // namespace cellular_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CELLULAR_SETUP_CELLULAR_SETUP_IMPL_H_
