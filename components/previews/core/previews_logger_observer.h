
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_LOGGER_OBSERVER_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_LOGGER_OBSERVER_H_

#include "base/macros.h"
#include "components/previews/core/previews_logger.h"

namespace previews {

// An interface for PreviewsLogger observer. This interface is for responding to
// events occur in PreviewsLogger (e.g. PreviewsLogger::LogMessage events).
class PreviewsLoggerObserver {
 public:
  // Notifies this observer that |message| has been added in PreviewsLogger.
  virtual void OnNewMessageLogAdded(
      const PreviewsLogger::MessageLog& message) = 0;

 protected:
  PreviewsLoggerObserver() {}
  virtual ~PreviewsLoggerObserver() {}
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_LOG_OBSERVER_H_
