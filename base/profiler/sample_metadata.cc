// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/sample_metadata.h"

#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"

namespace base {

ScopedSampleMetadata::ScopedSampleMetadata(base::StringPiece name,
                                           int64_t value)
    : name_hash_(HashMetricName(name)) {
  GetSampleMetadataRecorder()->Set(name_hash_, nullopt, value);
}

ScopedSampleMetadata::ScopedSampleMetadata(base::StringPiece name,
                                           int64_t key,
                                           int64_t value)
    : name_hash_(HashMetricName(name)), key_(key) {
  GetSampleMetadataRecorder()->Set(name_hash_, key, value);
}

ScopedSampleMetadata::~ScopedSampleMetadata() {
  GetSampleMetadataRecorder()->Remove(name_hash_, key_);
}

void SetSampleMetadata(base::StringPiece name, int64_t value) {
  GetSampleMetadataRecorder()->Set(base::HashMetricName(name), nullopt, value);
}

void SetSampleMetadata(base::StringPiece name, int64_t key, int64_t value) {
  GetSampleMetadataRecorder()->Set(base::HashMetricName(name), key, value);
}

void RemoveSampleMetadata(base::StringPiece name) {
  GetSampleMetadataRecorder()->Remove(base::HashMetricName(name), nullopt);
}

void RemoveSampleMetadata(base::StringPiece name, int64_t key) {
  GetSampleMetadataRecorder()->Remove(base::HashMetricName(name), key);
}

base::MetadataRecorder* GetSampleMetadataRecorder() {
  static base::NoDestructor<base::MetadataRecorder> instance;
  return instance.get();
}

}  // namespace base
