// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_UNIMPLEMENTED_H_
#define CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_UNIMPLEMENTED_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/ui/webui/help/version_updater.h"

// Temporary empty implementation used by platforms that have not specialized
// VersionUpdater.
class VersionUpdaterUnimplemented : public VersionUpdater {
 public:
  // VersionUpdater implementation.
  virtual void CheckForUpdate(const StatusCallback& callback) OVERRIDE {}
  virtual void RelaunchBrowser() const OVERRIDE {}

 protected:
  friend class VersionUpdater;

  // Clients must use VersionUpdater::Create().
  VersionUpdaterUnimplemented() {}
  virtual ~VersionUpdaterUnimplemented() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(VersionUpdaterUnimplemented);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HELP_VERSION_UPDATER_UNIMPLEMENTED_H_
