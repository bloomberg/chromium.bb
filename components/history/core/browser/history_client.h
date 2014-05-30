// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CLIENT_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CLIENT_H_

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace history {

// This class abstracts operations that depend on the embedder's environment,
// e.g. Chrome.
class HistoryClient : public KeyedService {
 protected:
  HistoryClient() {}

  DISALLOW_COPY_AND_ASSIGN(HistoryClient);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_CLIENT_H_
