// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_VERSION_HANDLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WEBUI_VERSION_HANDLER_CHROMEOS_H_

#include <string>

#include "chrome/browser/chromeos/version_loader.h"
#include "chrome/browser/ui/webui/version_handler.h"

// VersionHandlerChromeOS is responsible for loading the Chrome OS
// version.
class VersionHandlerChromeOS : public VersionHandler {
 public:
  VersionHandlerChromeOS();
  virtual ~VersionHandlerChromeOS();

  // VersionHandler overrides:
  virtual void HandleRequestVersionInfo(const base::ListValue* args) OVERRIDE;

  // Callback from chromeos::VersionLoader giving the version.
  void OnVersion(const std::string& version);

 private:
  // Handles asynchronously loading the version.
  chromeos::VersionLoader loader_;

  // Used to request the version.
  base::CancelableTaskTracker tracker_;

  DISALLOW_COPY_AND_ASSIGN(VersionHandlerChromeOS);
};

#endif  // CHROME_BROWSER_UI_WEBUI_VERSION_HANDLER_CHROMEOS_H_
