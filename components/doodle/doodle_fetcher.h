// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOODLE_DOODLE_FETCHER_H_
#define COMPONENTS_DOODLE_DOODLE_FETCHER_H_

#include "base/callback_forward.h"
#include "base/optional.h"
#include "components/doodle/doodle_types.h"

namespace doodle {

// Interface for fetching the current doodle from the network.
// It asynchronously calls a callback when fetching the doodle information from
// the remote enpoint finishes.
// DoodleFetcherImpl is the default implementation; this interface exists to
// make it easy to use fakes or mocks in tests.
class DoodleFetcher {
 public:
  // Callback that is invoked when the fetching is done.
  // |doodle_config| will only contain a value if |state| is AVAILABLE.
  using FinishedCallback = base::OnceCallback<void(
      DoodleState state,
      const base::Optional<DoodleConfig>& doodle_config)>;

  virtual ~DoodleFetcher() = default;

  // Fetches a doodle asynchronously. The |callback| is called with a
  // DoodleState indicating whether the request succeded in fetching a doodle.
  // If a fetch is already running, the callback will be queued and invoked with
  // the result from the next completed request.
  virtual void FetchDoodle(FinishedCallback callback) = 0;
};

}  // namespace doodle

#endif  // COMPONENTS_DOODLE_DOODLE_FETCHER_H_
