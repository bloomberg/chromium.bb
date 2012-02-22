// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_H_
#pragma once

#include <string>

#include "base/callback.h"

// Interface implemented to expose per-platform updating functionality.
class VersionUpdater {
 public:
#if defined(OS_CHROMEOS)
  typedef base::Callback<void(const std::string&)> ChannelCallback;
#endif

  // Update process state machine.
  enum Status {
    CHECKING,
    UPDATING,
    NEARLY_UPDATED,
    UPDATED
  };

  // Used to update the client of status changes. int parameter is the progress
  // and should only be non-zero for the UPDATING state.
  typedef base::Callback<void(Status, int)> StatusCallback;

  virtual ~VersionUpdater() {}

  // Sub-classes must implement this method to create the respective
  // specialization.
  static VersionUpdater* Create();

  // Begins the update process.  |callback| is called for each status update.
  virtual void CheckForUpdate(const StatusCallback& callback) = 0;

  // Relaunches the browser, generally after being updated.
  virtual void RelaunchBrowser() const = 0;

#if defined(OS_CHROMEOS)
  virtual void SetReleaseChannel(const std::string& channel) = 0;
  virtual void GetReleaseChannel(const ChannelCallback& callback) = 0;
#endif
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_H_
