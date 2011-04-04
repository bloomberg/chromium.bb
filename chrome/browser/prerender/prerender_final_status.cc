// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_final_status.h"

#include "base/metrics/histogram.h"

namespace prerender {

void RecordFinalStatus(FinalStatus final_status) {
  DCHECK(final_status != FINAL_STATUS_MAX);
  // FINAL_STATUS_CONTROL_GROUP indicates that the PrerenderContents
  // was created only to measure "would-have-been-prerendered" for
  // control group measurements. Don't pollute data with it.
  if (final_status == FINAL_STATUS_CONTROL_GROUP)
    return;
  UMA_HISTOGRAM_ENUMERATION("Prerender.FinalStatus",
                            final_status,
                            FINAL_STATUS_MAX);
}

}  // namespace prerender
