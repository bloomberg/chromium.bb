// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_H_

#include <string>

#include "base/callback.h"
#include "base/strings/string16.h"

// Interface implemented to expose per-platform updating functionality.
class VersionUpdater {
 public:
  // Update process state machine.
  enum Status {
    CHECKING,
    UPDATING,
    NEARLY_UPDATED,
    UPDATED,
    FAILED,
    FAILED_OFFLINE,
    FAILED_CONNECTION_TYPE_DISALLOWED,
    DISABLED,
  };

#if defined(OS_MACOSX)
  // Promotion state.
  enum PromotionState {
    PROMOTE_HIDDEN,
    PROMOTE_ENABLED,
    PROMOTE_DISABLED
  };
#endif  // defined(OS_MACOSX)

  // TODO(jhawkins): Use a delegate interface instead of multiple callback
  // types.
#if defined(OS_CHROMEOS)
  typedef base::Callback<void(const std::string&)> ChannelCallback;
#endif

  // Used to update the client of status changes. int parameter is the progress
  // and should only be non-zero for the UPDATING state.
  // base::string16 parameter is a message explaining a failure.
  typedef base::Callback<void(Status, int, const base::string16&)>
      StatusCallback;

#if defined(OS_MACOSX)
  // Used to show or hide the promote UI elements.
  typedef base::Callback<void(PromotionState)> PromoteCallback;
#endif

  virtual ~VersionUpdater() {}

  // Sub-classes must implement this method to create the respective
  // specialization.
  static VersionUpdater* Create();

  // Begins the update process by checking for update availability.
  // |status_callback| is called for each status update. |promote_callback| can
  // be used to show or hide the promote UI elements.
  virtual void CheckForUpdate(const StatusCallback& status_callback
#if defined(OS_MACOSX)
                              , const PromoteCallback& promote_callback
#endif
                              ) = 0;

#if defined(OS_MACOSX)
  // Make updates available for all users.
  virtual void PromoteUpdater() const = 0;
#endif

  // Relaunches the browser, generally after being updated.
  virtual void RelaunchBrowser() const = 0;

#if defined(OS_CHROMEOS)
  virtual void SetChannel(const std::string& channel,
                          bool is_powerwash_allowed) = 0;
  virtual void GetChannel(bool get_current_channel,
                          const ChannelCallback& callback) = 0;
#endif
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_H_
