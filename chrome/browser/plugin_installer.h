// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_INSTALLER_H_
#define CHROME_BROWSER_PLUGIN_INSTALLER_H_
#pragma once

#include "base/observer_list.h"
#include "base/string16.h"
#include "base/version.h"
#include "googleurl/src/gurl.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_item.h"
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

  // Information about a certain version of the plug-in.
  enum SecurityStatus {
    SECURITY_STATUS_UP_TO_DATE,
    SECURITY_STATUS_OUT_OF_DATE,
    SECURITY_STATUS_REQUIRES_AUTHORIZATION,
  };

  PluginInstaller(const std::string& identifier,
                  const string16& name,
                  bool url_for_display,
                  const GURL& plugin_url,
                  const GURL& help_url);
  virtual ~PluginInstaller();

  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;

  virtual void OnDownloadOpened(content::DownloadItem* download) OVERRIDE;

  void AddObserver(PluginInstallerObserver* observer);
  void RemoveObserver(PluginInstallerObserver* observer);

  void AddWeakObserver(WeakPluginInstallerObserver* observer);
  void RemoveWeakObserver(WeakPluginInstallerObserver* observer);

  // Unique identifier for the plug-in.
  const std::string& identifier() const { return identifier_; }

  // Human-readable name of the plug-in.
  const string16& name() const { return name_; }

  // If |url_for_display| is false, |plugin_url| is the URL of the download page
  // for the plug-in, which should be opened in a new tab. If it is true,
  // |plugin_url| is the URL of the plug-in installer binary, which can be
  // directly downloaded.
  bool url_for_display() const { return url_for_display_; }
  const GURL& plugin_url() const { return plugin_url_; }

  // URL to open when the user clicks on the "Problems installing?" link.
  const GURL& help_url() const { return help_url_; }

  InstallerState state() const { return state_; }

  // Adds information about a plug-in version.
  void AddVersion(const Version& version, SecurityStatus status);

  // Returns the security status for the given plug-in (i.e. whether it is
  // considered out-of-date, etc.)
  SecurityStatus GetSecurityStatus(const webkit::WebPluginInfo& plugin) const;

  // Opens the download URL in a new tab. This method should only be called if
  // |url_for_display| returns true.
  void OpenDownloadURL(content::WebContents* web_contents);

  // Starts downloading the download URL and opens the downloaded file
  // when finished. This method should only be called if |url_for_display|
  // returns false.
  void StartInstalling(TabContents* tab_contents);

  // If |status_str| describes a valid security status, writes it to |status|
  // and returns true, else returns false and leaves |status| unchanged.
  static bool ParseSecurityStatus(const std::string& status_str,
                                  SecurityStatus* status);

 private:
  struct VersionComparator {
    bool operator() (const Version& lhs, const Version& rhs) const;
  };

  void DownloadStarted(scoped_refptr<content::DownloadManager> dlm,
                       content::DownloadId download_id,
                       net::Error error);
  void DownloadError(const std::string& msg);
  void DownloadCancelled();

  std::string identifier_;
  string16 name_;
  bool url_for_display_;
  GURL plugin_url_;
  GURL help_url_;
  std::map<Version, SecurityStatus, VersionComparator> versions_;

  InstallerState state_;
  ObserverList<PluginInstallerObserver> observers_;
  ObserverList<WeakPluginInstallerObserver> weak_observers_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstaller);
};

#endif  // CHROME_BROWSER_PLUGIN_INSTALLER_H_
