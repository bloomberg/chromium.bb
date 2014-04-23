// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_INSTALLER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_INSTALLER_H_

#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "base/version.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "url/gurl.h"

class PluginInstallerObserver;
class WeakPluginInstallerObserver;

namespace content {
class DownloadManager;
class WebContents;
struct WebPluginInfo;
}

class PluginInstaller : public content::DownloadItem::Observer {
 public:
  enum InstallerState {
    INSTALLER_STATE_IDLE,
    INSTALLER_STATE_DOWNLOADING,
  };

  PluginInstaller();
  virtual ~PluginInstaller();

  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadDestroyed(content::DownloadItem* download) OVERRIDE;

  void AddObserver(PluginInstallerObserver* observer);
  void RemoveObserver(PluginInstallerObserver* observer);

  void AddWeakObserver(WeakPluginInstallerObserver* observer);
  void RemoveWeakObserver(WeakPluginInstallerObserver* observer);

  InstallerState state() const { return state_; }

  // Opens the download URL in a new tab.
  void OpenDownloadURL(const GURL& plugin_url,
                       content::WebContents* web_contents);

  // Starts downloading the download URL and opens the downloaded file
  // when finished.
  void StartInstalling(const GURL& plugin_url,
                       content::WebContents* web_contents);

 private:
  void StartInstallingWithDownloadManager(
      const GURL& plugin_url,
      content::WebContents* web_contents,
      content::DownloadManager* download_manager);
  void DownloadStarted(content::DownloadItem* item,
                       content::DownloadInterruptReason interrupt_reason);
  void DownloadError(const std::string& msg);
  void DownloadCancelled();

  InstallerState state_;
  ObserverList<PluginInstallerObserver> observers_;
  int strong_observer_count_;
  ObserverList<WeakPluginInstallerObserver> weak_observers_;

  FRIEND_TEST_ALL_PREFIXES(PluginInstallerTest,
                           StartInstalling_SuccessfulDownload);
  FRIEND_TEST_ALL_PREFIXES(PluginInstallerTest, StartInstalling_FailedStart);
  FRIEND_TEST_ALL_PREFIXES(PluginInstallerTest, StartInstalling_Interrupted);
  DISALLOW_COPY_AND_ASSIGN(PluginInstaller);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_INSTALLER_H_
