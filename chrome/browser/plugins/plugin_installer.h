// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_INSTALLER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_INSTALLER_H_

#include "base/observer_list.h"
#include "base/string16.h"
#include "base/version.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "content/public/browser/download_item.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"

class PluginInstallerObserver;
class WeakPluginInstallerObserver;

namespace content {
class WebContents;
}

namespace webkit {
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
  void DownloadStarted(scoped_refptr<content::DownloadManager> dlm,
                       content::DownloadItem* item,
                       net::Error error);
  void DownloadError(const std::string& msg);
  void DownloadCancelled();

  InstallerState state_;
  ObserverList<PluginInstallerObserver> observers_;
  ObserverList<WeakPluginInstallerObserver> weak_observers_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstaller);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_INSTALLER_H_
