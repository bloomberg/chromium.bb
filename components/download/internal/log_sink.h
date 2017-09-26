// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_LOG_SINK_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_LOG_SINK_H_

#include "components/download/internal/controller.h"
#include "components/download/internal/startup_status.h"

namespace download {

// A destination for all interesting events from internal components.
class LogSink {
 public:
  virtual ~LogSink() = default;

  // To be called whenever the StartupStatus/Controller::State changes.
  virtual void OnServiceStatusChanged() = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_LOG_SINK_H_
