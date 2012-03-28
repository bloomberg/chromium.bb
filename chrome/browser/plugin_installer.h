// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_INSTALLER_H_
#define CHROME_BROWSER_PLUGIN_INSTALLER_H_
#pragma once

#include "base/observer_list.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_item.h"
#include "net/base/net_errors.h"

class FilePath;
class PluginInstallerObserver;
class TabContentsWrapper;
class WeakPluginInstallerObserver;

namespace content {
class WebContents;
}

class PluginInstaller : public content::DownloadItem::Observer {
 public:
  enum State {
    kStateIdle,
    kStateDownloading,
  };

  PluginInstaller(const std::string& identifier,
                  const GURL& plugin_url,
                  const GURL& help_url,
                  const string16& name,
                  bool url_for_display,
                  bool requires_authorization);
  virtual ~PluginInstaller();

  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;

  virtual void OnDownloadOpened(content::DownloadItem* download) OVERRIDE;

  void AddObserver(PluginInstallerObserver* observer);
  void RemoveObserver(PluginInstallerObserver* observer);

  void AddWeakObserver(WeakPluginInstallerObserver* observer);
  void RemoveWeakObserver(WeakPluginInstallerObserver* observer);

  State state() const { return state_; }

  // Unique identifier for the plug-in. Should be kept in sync with the
  // identifier in plugin_list.cc.
  const std::string& identifier() const { return identifier_; }

  // Human-readable name of the plug-in.
  const string16& name() const { return name_; }

  // Whether the plug-in requires user authorization to run.
  bool requires_authorization() const { return requires_authorization_; }

  // If |url_for_display| is false, |plugin_url| is the URL of the download page
  // for the plug-in, which should be opened in a new tab. If it is true,
  // |plugin_url| is the URL of the plug-in installer binary, which can be
  // directly downloaded.
  bool url_for_display() const { return url_for_display_; }
  const GURL& plugin_url() const { return plugin_url_; }

  // URL to open when the user clicks on the "Problems installing?" link.
  const GURL& help_url() const { return help_url_; }

  // Opens the download URL in a new tab. This method should only be called if
  // |url_for_display| returns true.
  void OpenDownloadURL(content::WebContents* web_contents);

  // Starts downloading the download URL and opens the downloaded file
  // when finished. This method should only be called if |url_for_display|
  // returns false.
  void StartInstalling(TabContentsWrapper* wrapper);

 private:
  void DownloadStarted(scoped_refptr<content::DownloadManager> dlm,
                       content::DownloadId download_id,
                       net::Error error);
  void DownloadError(const std::string& msg);
  void DownloadCancelled();

  State state_;
  ObserverList<PluginInstallerObserver> observers_;
  ObserverList<WeakPluginInstallerObserver> weak_observers_;

  std::string identifier_;
  GURL plugin_url_;
  GURL help_url_;
  string16 name_;
  bool url_for_display_;
  bool requires_authorization_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstaller);
};

#endif  // CHROME_BROWSER_PLUGIN_INSTALLER_H_
