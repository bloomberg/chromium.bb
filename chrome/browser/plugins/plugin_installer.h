// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_INSTALLER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_INSTALLER_H_

#include "base/observer_list.h"
#include "base/string16.h"
#include "base/version.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_item.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"

class FilePath;
class PluginInstallerObserver;
class TabContents;
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

  explicit PluginInstaller(PluginMetadata* plugin);
  virtual ~PluginInstaller();

  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadDestroyed(content::DownloadItem* download) OVERRIDE;

  void AddObserver(PluginInstallerObserver* observer);
  void RemoveObserver(PluginInstallerObserver* observer);

  void AddWeakObserver(WeakPluginInstallerObserver* observer);
  void RemoveWeakObserver(WeakPluginInstallerObserver* observer);

  // Unique identifier for the plug-in.
  const std::string& identifier() const { return plugin_->identifier(); }

  // Human-readable name of the plug-in.
  const string16& name() const { return plugin_->name(); }

  // If |url_for_display| is false, |plugin_url| is the URL of the download page
  // for the plug-in, which should be opened in a new tab. If it is true,
  // |plugin_url| is the URL of the plug-in installer binary, which can be
  // directly downloaded.
  bool url_for_display() const { return plugin_->url_for_display(); }
  const GURL& plugin_url() const { return plugin_->plugin_url(); }

  // URL to open when the user clicks on the "Problems installing?" link.
  const GURL& help_url() const { return plugin_->help_url(); }

  InstallerState state() const { return state_; }

  // Opens the download URL in a new tab. This method should only be called if
  // |url_for_display| returns true.
  void OpenDownloadURL(content::WebContents* web_contents);

  // Starts downloading the download URL and opens the downloaded file
  // when finished. This method should only be called if |url_for_display|
  // returns false.
  void StartInstalling(TabContents* tab_contents);

 private:
  void DownloadStarted(scoped_refptr<content::DownloadManager> dlm,
                       content::DownloadId download_id,
                       net::Error error);
  void DownloadError(const std::string& msg);
  void DownloadCancelled();

  // Can't be NULL. It is owned by PluginFinder.
  PluginMetadata* plugin_;

  InstallerState state_;
  ObserverList<PluginInstallerObserver> observers_;
  ObserverList<WeakPluginInstallerObserver> weak_observers_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstaller);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_INSTALLER_H_
