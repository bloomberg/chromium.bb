// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_CHROMEOS_H_
#pragma once

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
  virtual void SetReleaseChannel(const std::string& channel) OVERRIDE;
  virtual void GetReleaseChannel(const ChannelCallback& callback) OVERRIDE;

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

  // Callback from UpdateEngineClient::GetReleaseTrack().
  void UpdateSelectedChannel(const std::string& channel);

  // Callback used to communicate update status to the client.
  StatusCallback callback_;

  // Callback used to communicate current channel to the client.
  ChannelCallback channel_callback_;

  base::WeakPtrFactory<VersionUpdaterCros> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VersionUpdaterCros);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_CHROMEOS_H_
