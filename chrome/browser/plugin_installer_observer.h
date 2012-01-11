// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_INSTALLER_OBSERVER_H_
#define CHROME_BROWSER_PLUGIN_INSTALLER_OBSERVER_H_
#pragma once

#include <string>

class PluginInstaller;

class PluginInstallerObserver {
 public:
  explicit PluginInstallerObserver(PluginInstaller* installer);
  virtual ~PluginInstallerObserver();

 protected:
  PluginInstaller* installer() { return installer_; }

 private:
  friend class PluginInstaller;

  virtual void DidStartDownload();
  virtual void DidFinishDownload();
  virtual void DownloadError(const std::string& message);

  // Weak pointer; Owned by PluginFinder, which is a singleton.
  PluginInstaller* installer_;
};

#endif  // CHROME_BROWSER_PLUGIN_INSTALLER_OBSERVER_H_
