// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_LOGGER_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_LOGGER_H_

#include <memory>

#include "base/macros.h"

namespace base {
class Value;
}

namespace download {

// A helper class to expose internals of the downloads system to a logging
// component and/or debug UI.
class Logger {
 public:
  // An Observer to be notified of any DownloadService changes.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called whenever the status of the DownloadService changes.  This will
    // have the same data as |GetServiceStatus()|.
    virtual void OnServiceStatusChanged(const base::Value& service_status) = 0;
  };

  virtual ~Logger() = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the current status of the Download Service.  The serialized format
  // will be:
  // {
  //   serviceState: string [CREATED,INITIALIZING,READY,RECOVERING,UNAVAILABLE],
  //   modelStatus: string [OK,BAD,UNKNOWN],
  //   driverStatus: string [OK,BAD,UNKNOWN],
  //   fileMonitorStatus: string [OK,BAD,UNKNOWN]
  // }
  virtual base::Value GetServiceStatus() = 0;

 protected:
  Logger() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(Logger);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_LOGGER_H_
