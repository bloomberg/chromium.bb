// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_interface.h"

#include "base/memory/ptr_util.h"
#include "components/ukm/public/ukm_recorder.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace ukm {

UkmInterface::UkmInterface(UkmRecorder* ukm_recorder)
    : ukm_recorder_(ukm_recorder) {}

UkmInterface::~UkmInterface() = default;

// static
void UkmInterface::Create(UkmRecorder* ukm_recorder,
                          const service_manager::BindSourceInfo& source_info,
                          mojom::UkmRecorderInterfaceRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<UkmInterface>(ukm_recorder),
                          std::move(request));
}

void UkmInterface::AddEntry(mojom::UkmEntryPtr ukm_entry) {
  ukm_recorder_->AddEntry(std::move(ukm_entry));
}

}  // namespace ukm
