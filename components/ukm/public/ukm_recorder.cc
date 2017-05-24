// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/public/ukm_recorder.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "components/ukm/public/ukm_entry_builder.h"

namespace ukm {

UkmRecorder* g_ukm_recorder = nullptr;

const base::Feature kUkmFeature = {"Ukm", base::FEATURE_DISABLED_BY_DEFAULT};

UkmRecorder::UkmRecorder() {
  DCHECK(!g_ukm_recorder);
  g_ukm_recorder = this;
}

UkmRecorder::~UkmRecorder() {
  g_ukm_recorder = nullptr;
}

// static
UkmRecorder* UkmRecorder::Get() {
  return g_ukm_recorder;
}

// static
ukm::SourceId UkmRecorder::GetNewSourceID() {
  static base::StaticAtomicSequenceNumber seq;
  return static_cast<ukm::SourceId>(seq.GetNext());
}

std::unique_ptr<UkmEntryBuilder> UkmRecorder::GetEntryBuilder(
    ukm::SourceId source_id,
    const char* event_name) {
  return base::MakeUnique<UkmEntryBuilder>(
      base::Bind(&UkmRecorder::AddEntry, base::Unretained(this)), source_id,
      event_name);
}

}  // namespace ukm
