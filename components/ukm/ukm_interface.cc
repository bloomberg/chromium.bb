// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_interface.h"

#include "base/atomic_sequence_num.h"
#include "base/memory/ptr_util.h"
#include "components/ukm/public/ukm_recorder.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "url/gurl.h"

namespace ukm {

namespace {

// Map source ids from different instances into unique namespaces, so that
// clients can create thier own IDs without having them collide.
// This won't be necessary once we switch to using CoordinationUnitIDs.
int64_t ConvertSourceId(int64_t source_id, int64_t instance_id) {
  const int64_t low_bits = (INT64_C(1) << 32) - 1;
  // Neither ID should get large enough to cause an issue, but explicitly
  // discard down to 32 bits anyway.
  return ((instance_id & low_bits) << 32) | (source_id & low_bits);
}

}  // namespace

UkmInterface::UkmInterface(UkmRecorder* ukm_recorder, int64_t instance_id)
    : ukm_recorder_(ukm_recorder), instance_id_(instance_id) {}

UkmInterface::~UkmInterface() = default;

// static
void UkmInterface::Create(UkmRecorder* ukm_recorder,
                          const service_manager::BindSourceInfo& source_info,
                          mojom::UkmRecorderInterfaceRequest request) {
  static base::StaticAtomicSequenceNumber seq;
  mojo::MakeStrongBinding(
      base::MakeUnique<UkmInterface>(ukm_recorder,
                                     static_cast<int64_t>(seq.GetNext()) + 1),
      std::move(request));
}

void UkmInterface::AddEntry(mojom::UkmEntryPtr ukm_entry) {
  ukm_entry->source_id = ConvertSourceId(instance_id_, ukm_entry->source_id);
  ukm_recorder_->AddEntry(std::move(ukm_entry));
}

void UkmInterface::UpdateSourceURL(int64_t source_id, const std::string& url) {
  ukm_recorder_->UpdateSourceURL(ConvertSourceId(instance_id_, source_id),
                                 GURL(url));
}

}  // namespace ukm
