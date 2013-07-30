// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"

namespace {

// Name of field trial for that sends gzipped UMA protobufs.
const char kTrialName[] = "GZippedProtobufs";

// Divisor for the gzipped protobufs trial.
const int kTrialDivisor = 100;

// Quotient for the gzipped protobufs trial.
const int kTrialQuotient = 50;

// Name of the group with gzipped protobufs.
const char kGroupName[] = "GzippedProtobufsEnabled";

// Name of the group with non-gzipped protobufs.
const char kControlGroupName[] = "GzippedProtobufsDisabled";

// This is set to true if we land in the Finch group that will be sending
// protobufs after compressing them.
bool should_gzip_protobufs = false;

}  // namespace

namespace metrics {

void CreateGzippedProtobufsFieldTrial() {
  scoped_refptr<base::FieldTrial> gzipped_protobufs_trial =
      base::FieldTrialList::FactoryGetFieldTrial(
          kTrialName,
          kTrialDivisor,
          kControlGroupName,
          2013,
          10,
          1,
          base::FieldTrial::ONE_TIME_RANDOMIZED,
          NULL);
  int gzipped_protobufs_group = gzipped_protobufs_trial->AppendGroup(
      kGroupName,
      kTrialQuotient);
  should_gzip_protobufs =
      gzipped_protobufs_trial->group() == gzipped_protobufs_group;
}

bool ShouldGzipProtobufsBeforeUploading() {
  return should_gzip_protobufs;
}

}  // namespace metrics
