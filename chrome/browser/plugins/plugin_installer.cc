// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_installer.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/process/process.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/plugins/plugin_installer_observer.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/web_contents.h"

using content::DownloadItem;

PluginInstaller::PluginInstaller() : strong_observer_count_(0) {}

PluginInstaller::~PluginInstaller() {
}

void PluginInstaller::AddObserver(PluginInstallerObserver* observer) {
  strong_observer_count_++;
  observers_.AddObserver(observer);
}

void PluginInstaller::RemoveObserver(PluginInstallerObserver* observer) {
  strong_observer_count_--;
  observers_.RemoveObserver(observer);
  if (strong_observer_count_ == 0) {
    for (WeakPluginInstallerObserver& observer : weak_observers_)
      observer.OnlyWeakObserversLeft();
  }
}

void PluginInstaller::AddWeakObserver(WeakPluginInstallerObserver* observer) {
  weak_observers_.AddObserver(observer);
}

void PluginInstaller::RemoveWeakObserver(
    WeakPluginInstallerObserver* observer) {
  weak_observers_.RemoveObserver(observer);
}

void PluginInstaller::OpenDownloadURL(const GURL& plugin_url,
                                      content::WebContents* web_contents) {
  web_contents->OpenURL(content::OpenURLParams(
      plugin_url, content::Referrer(web_contents->GetURL(),
                                    blink::WebReferrerPolicyDefault),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_TYPED,
      false));
  for (PluginInstallerObserver& observer : observers_)
    observer.DownloadFinished();
}
