// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_OBSERVER_H_
#define CHROME_BROWSER_PLUGIN_OBSERVER_H_
#pragma once

#include "chrome/browser/tab_contents/tab_contents_observer.h"

class FilePath;
class GURL;
class PluginInstallerInfoBarDelegate;
class TabContents;

class PluginObserver : public TabContentsObserver {
 public:
  explicit PluginObserver(TabContents* tab_contents);
  ~PluginObserver();

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

 private:
  // Returns the PluginInstallerInfoBarDelegate, creating it if necessary.
  PluginInstallerInfoBarDelegate* GetPluginInstaller();

  void OnMissingPluginStatus(int status);
  void OnCrashedPlugin(const FilePath& plugin_path);
  void OnBlockedOutdatedPlugin(const string16& name, const GURL& update_url);

  TabContents* tab_contents_;  // Weak, owns us.
  // PluginInstallerInfoBarDelegate, lazily created.
  scoped_ptr<PluginInstallerInfoBarDelegate> plugin_installer_;

  DISALLOW_COPY_AND_ASSIGN(PluginObserver);
};

#endif  // CHROME_BROWSER_PLUGIN_OBSERVER_H_
