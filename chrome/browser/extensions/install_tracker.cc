// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_tracker.h"

namespace extensions {

InstallTracker::InstallTracker() {
}

InstallTracker::~InstallTracker() {
}

void InstallTracker::AddObserver(InstallObserver* observer) {
  observers_.AddObserver(observer);
}

void InstallTracker::RemoveObserver(InstallObserver* observer) {
  observers_.RemoveObserver(observer);
}

void InstallTracker::OnBeginExtensionInstall(
    const std::string& extension_id,
    const std::string& extension_name,
    const gfx::ImageSkia& installing_icon,
    bool is_app) {
  FOR_EACH_OBSERVER(InstallObserver, observers_,
                    OnBeginExtensionInstall(extension_id,
                                            extension_name,
                                            installing_icon,
                                            is_app));
}

void InstallTracker::OnDownloadProgress(const std::string& extension_id,
                                        int percent_downloaded) {
  FOR_EACH_OBSERVER(InstallObserver, observers_,
                    OnDownloadProgress(extension_id, percent_downloaded));
}

void InstallTracker::OnInstallFailure(
    const std::string& extension_id) {
  FOR_EACH_OBSERVER(InstallObserver, observers_,
                    OnInstallFailure(extension_id));
}

}  // namespace extensions
