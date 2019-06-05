// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/ota_activator.h"

#include <utility>

namespace chromeos {

namespace cellular_setup {

OtaActivator::OtaActivator(base::OnceClosure on_finished_callback)
    : on_finished_callback_(std::move(on_finished_callback)), binding_(this) {}

OtaActivator::~OtaActivator() = default;

mojom::CarrierPortalHandlerPtr OtaActivator::GenerateInterfacePtr() {
  // Only one InterfacePtr should be created per instance.
  DCHECK(!binding_);

  mojom::CarrierPortalHandlerPtr interface_ptr;
  binding_.Bind(mojo::MakeRequest(&interface_ptr));
  return interface_ptr;
}

void OtaActivator::InvokeOnFinishedCallback() {
  DCHECK(on_finished_callback_);
  std::move(on_finished_callback_).Run();
}

}  // namespace cellular_setup

}  // namespace chromeos
