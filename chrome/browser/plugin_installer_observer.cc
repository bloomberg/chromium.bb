// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugin_installer_observer.h"

#include "chrome/browser/plugin_installer.h"

PluginInstallerObserver::PluginInstallerObserver(PluginInstaller* installer)
    : installer_(installer) {
  if (installer)
    installer->AddObserver(this);
}

PluginInstallerObserver::~PluginInstallerObserver() {
  if (installer_)
    installer_->RemoveObserver(this);
}

void PluginInstallerObserver::DidStartDownload() {
}

void PluginInstallerObserver::DidFinishDownload() {
}

