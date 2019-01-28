// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/metrics.h"

#include "base/metrics/histogram_macros.h"

namespace autofill_assistant {

namespace {
const char* const kDropOutEnumName = "Android.AutofillAssistant.DropOutReason";
}  // namespace

// static
void Metrics::RecordDropOut(DropOutReason reason) {
  DCHECK_LE(reason, DropOutReason::NUM_ENTRIES);
  UMA_HISTOGRAM_ENUMERATION(kDropOutEnumName, reason, NUM_ENTRIES);
}

}  // namespace autofill_assistant
