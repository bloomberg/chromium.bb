// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_LOG_SOURCE_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_LOG_SOURCE_H_

#include "components/download/internal/controller.h"

namespace download {

struct StartupStatus;

// A source for all relevant logging data.  LoggerImpl will pull from an
// instance of LogSource to push relevant log information to observers.
class LogSource {
 public:
  virtual ~LogSource() = default;

  // Returns the state of the Controller (see Controller::State).
  virtual Controller::State GetControllerState() = 0;

  // Returns the current StartupStatus of the service.
  virtual const StartupStatus& GetStartupStatus() = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_LOG_SOURCE_H_
