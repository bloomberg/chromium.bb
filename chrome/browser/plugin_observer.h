// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_OBSERVER_H_
#define CHROME_BROWSER_PLUGIN_OBSERVER_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "content/browser/tab_contents/tab_contents_observer.h"

class GURL;
class InfoBarDelegate;
class PluginInstaller;
class TabContentsWrapper;

class PluginObserver : public TabContentsObserver {
 public:
  explicit PluginObserver(TabContentsWrapper* tab_contents);
  virtual ~PluginObserver();

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  void OnBlockedOutdatedPlugin(const string16& name, const GURL& update_url);
  void OnFindMissingPlugin(int placeholder_id, const std::string& mime_type);

  void FoundMissingPlugin(int placeholder_id,
                          const std::string& mime_type,
                          PluginInstaller* installer);
  void DidNotFindMissingPlugin(int placeholder_id,
                               const std::string& mime_type);
  void InstallMissingPlugin(PluginInstaller* installer);

  base::WeakPtrFactory<PluginObserver> weak_ptr_factory_;

  TabContentsWrapper* tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(PluginObserver);
};

#endif  // CHROME_BROWSER_PLUGIN_OBSERVER_H_
