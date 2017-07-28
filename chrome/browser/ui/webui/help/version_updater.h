// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_H_

#include <string>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "build/build_config.h"

#if defined(OS_CHROMEOS)
#include "third_party/cros_system_api/dbus/update_engine/dbus-constants.h"
#endif  // defined(OS_CHROMEOS)

namespace content {
class WebContents;
}

// Interface implemented to expose per-platform updating functionality.
class VersionUpdater {
 public:
  // Update process state machine.
  enum Status {
    CHECKING,
    NEED_PERMISSION_TO_UPDATE,
    UPDATING,
    NEARLY_UPDATED,
    UPDATED,
    FAILED,
    FAILED_OFFLINE,
    FAILED_CONNECTION_TYPE_DISALLOWED,
    DISABLED,
    DISABLED_BY_ADMIN
  };

  // Promotion state (Mac-only).
  enum PromotionState {
    PROMOTE_HIDDEN,
    PROMOTE_ENABLED,
    PROMOTE_DISABLED,
    PROMOTED,
  };

  // TODO(jhawkins): Use a delegate interface instead of multiple callback
  // types.
#if defined(OS_CHROMEOS)
  typedef base::Callback<void(const std::string&)> ChannelCallback;
  typedef base::Callback<void(update_engine::EndOfLifeStatus status)>
      EolStatusCallback;
#endif

  // Used to update the client of status changes. int parameter is the progress
  // and should only be non-zero for the UPDATING state.
  // std::string parameter is the version of the available update and should be
  // empty string when update is not available.
  // int64_t parameter is the size in bytes of the available update and should
  // be 0 when update is not available.
  // base::string16 parameter is a message explaining a failure.
  typedef base::Callback<
      void(Status, int, const std::string&, int64_t, const base::string16&)>
      StatusCallback;

  // Used to show or hide the promote UI elements. Mac-only.
  typedef base::Callback<void(PromotionState)> PromoteCallback;

  virtual ~VersionUpdater() {}

  // Sub-classes must implement this method to create the respective
  // specialization. |web_contents| may be null, in which case any required UX
  // (e.g., UAC to elevate on Windows) may not be associated with any existing
  // browser windows.
  static VersionUpdater* Create(content::WebContents* web_contents);

  // Begins the update process by checking for update availability.
  // |status_callback| is called for each status update. |promote_callback|
  // (which is only used on the Mac) can be used to show or hide the promote UI
  // elements.
  virtual void CheckForUpdate(const StatusCallback& status_callback,
                              const PromoteCallback& promote_callback) = 0;

#if defined(OS_MACOSX)
  // Make updates available for all users.
  virtual void PromoteUpdater() const = 0;
#endif

#if defined(OS_CHROMEOS)
  virtual void SetChannel(const std::string& channel,
                          bool is_powerwash_allowed) = 0;
  virtual void GetChannel(bool get_current_channel,
                          const ChannelCallback& callback) = 0;
  virtual void GetEolStatus(const EolStatusCallback& callback) = 0;

  // Sets a one time permission on a certain update in Update Engine.
  // - update_version: the Chrome OS version we want to update to.
  // - update_size: the size of that Chrome OS version in bytes.
  // These two parameters are a failsafe to prevent downloading an update that
  // the user didn't agree to. They should be set using the version and size we
  // received from update engine when it broadcasts NEED_PERMISSION_TO_UPDATE.
  // They are used by update engine to double-check with update server in case
  // there's a new update available or a delta update becomes a full update with
  // a larger size.
  virtual void SetUpdateOverCellularOneTimePermission(
      const StatusCallback& callback,
      const std::string& update_version,
      int64_t update_size) = 0;
#endif
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_H_
