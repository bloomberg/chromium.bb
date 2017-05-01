// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/swap_metrics_observer.h"

namespace content {

// static
SwapMetricsObserver* SwapMetricsObserver::GetInstance() {
  // SwapMetricsObserver isn't available on Windows for now.
  // TODO(bashi): Figure out a way to measure swap rates on Windows.
  return nullptr;
}

}  // namespace content
