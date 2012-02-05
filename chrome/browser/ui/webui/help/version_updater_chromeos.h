// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_CHROMEOS_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/dbus/update_engine_client.h"
#include "chrome/browser/ui/webui/help/version_updater.h"

class VersionUpdaterCros : public VersionUpdater,
                           public chromeos::UpdateEngineClient::Observer {
 public:
  // VersionUpdater implementation.
  virtual void CheckForUpdate(const StatusCallback& callback) OVERRIDE;
  virtual void RelaunchBrowser() const OVERRIDE;

 protected:
  friend class VersionUpdater;

  // Clients must use VersionUpdater::Create().
  VersionUpdaterCros();
  virtual ~VersionUpdaterCros();

 private:
  // UpdateEngineClient::Observer implementation.
  virtual void UpdateStatusChanged(
      const chromeos::UpdateEngineClient::Status& status) OVERRIDE;

  // Callback used to communicate update status to the client.
  StatusCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(VersionUpdaterCros);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_CHROMEOS_H_
