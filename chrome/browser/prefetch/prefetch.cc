// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prefetch/prefetch_field_trial.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "net/base/network_change_notifier.h"

namespace prefetch {

bool IsPrefetchEnabled(content::ResourceContext* resource_context) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  DCHECK(io_data);

  // TODO(bnc): Remove this condition once the new
  // predictive preference is used on all platforms. See crbug.com/334602.
  if (io_data->network_prediction_options()->GetValue() ==
          chrome_browser_net::NETWORK_PREDICTION_UNSET &&
      net::NetworkChangeNotifier::IsConnectionCellular(
          net::NetworkChangeNotifier::GetConnectionType())) {
    return false;
  }

  return chrome_browser_net::CanPredictNetworkActionsIO(io_data) &&
         !DisableForFieldTrial();
}

}  // namespace prefetch
