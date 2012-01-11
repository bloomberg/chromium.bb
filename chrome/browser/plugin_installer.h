// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_INSTALLER_H_
#define CHROME_BROWSER_PLUGIN_INSTALLER_H_
#pragma once

#include "base/observer_list.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"

class FilePath;
class PluginInstallerObserver;

namespace net {
class URLRequestContextGetter;
}

class PluginInstaller {
 public:
  enum State {
    kStateIdle,
    kStateDownloading,
  };

  PluginInstaller(const std::string& identifier,
                  const GURL& plugin_url,
                  const GURL& help_url,
                  const string16& name,
                  bool url_for_display);
  ~PluginInstaller();

  void AddObserver(PluginInstallerObserver* observer);
  void RemoveObserver(PluginInstallerObserver* observer);

  State state() const { return state_; }

  // Unique identifier for the plug-in. Should be kept in sync with the
  // identifier in plugin_list.cc.
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

  void StartInstalling(net::URLRequestContextGetter* request_context);

 private:
  void DidFinishDownload(const FilePath& downloaded_file);

  State state_;
  ObserverList<PluginInstallerObserver> observers_;

  std::string identifier_;
  GURL plugin_url_;
  GURL help_url_;
  string16 name_;
  bool url_for_display_;

  DISALLOW_COPY_AND_ASSIGN(PluginInstaller);
};

#endif  // CHROME_BROWSER_PLUGIN_INSTALLER_H_
