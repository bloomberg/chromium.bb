// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "chrome/browser/prefetch/prefetch_field_trial.h"
#include "chrome/browser/profiles/profile_io_data.h"

namespace prefetch {

bool IsPrefetchEnabled(content::ResourceContext* resource_context) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  if (io_data != NULL && io_data->network_prediction_enabled()->GetValue())
    return IsPrefetchFieldTrialEnabled();
  return false;
}

}  // namespace prefetch
