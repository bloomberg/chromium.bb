// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_CHROMEOS_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/help/version_updater.h"
#include "chromeos/dbus/update_engine_client.h"

class VersionUpdaterCros : public VersionUpdater,
                           public chromeos::UpdateEngineClient::Observer {
 public:
  // VersionUpdater implementation.
  virtual void CheckForUpdate(const StatusCallback& callback) OVERRIDE;
  virtual void RelaunchBrowser() const OVERRIDE;
  virtual void SetChannel(const std::string& channel,
                          bool is_powerwash_allowed) OVERRIDE;
  virtual void GetChannel(bool get_current_channel,
                          const ChannelCallback& callback) OVERRIDE;

 protected:
  friend class VersionUpdater;

  // Clients must use VersionUpdater::Create().
  VersionUpdaterCros();
  virtual ~VersionUpdaterCros();

 private:
  // UpdateEngineClient::Observer implementation.
  virtual void UpdateStatusChanged(
      const chromeos::UpdateEngineClient::Status& status) OVERRIDE;

  // Callback from UpdateEngineClient::RequestUpdateCheck().
  void OnUpdateCheck(chromeos::UpdateEngineClient::UpdateCheckResult result);

  // Callback used to communicate update status to the client.
  StatusCallback callback_;

  // Last state received via UpdateStatusChanged().
  chromeos::UpdateEngineClient::UpdateStatusOperation last_operation_;

  base::WeakPtrFactory<VersionUpdaterCros> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VersionUpdaterCros);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_CHROMEOS_H_
