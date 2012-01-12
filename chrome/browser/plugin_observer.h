// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_OBSERVER_H_
#define CHROME_BROWSER_PLUGIN_OBSERVER_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "base/memory/scoped_vector.h"
#endif

class GURL;
class InfoBarDelegate;
class PluginInstaller;
class TabContentsWrapper;

#if defined(ENABLE_PLUGIN_INSTALLATION)
class PluginInstaller;
#endif

class PluginObserver : public content::WebContentsObserver {
 public:
  explicit PluginObserver(TabContentsWrapper* tab_contents);
  virtual ~PluginObserver();

  // content::WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
#if defined(ENABLE_PLUGIN_INSTALLATION)
  class MissingPluginHost;
#endif

  void OnBlockedOutdatedPlugin(const string16& name, const GURL& update_url);
#if defined(ENABLE_PLUGIN_INSTALLATION)
  void OnFindMissingPlugin(int placeholder_id, const std::string& mime_type);

  void FoundMissingPlugin(int placeholder_id,
                          const std::string& mime_type,
                          PluginInstaller* installer);
  void DidNotFindMissingPlugin(int placeholder_id);
  void InstallMissingPlugin(PluginInstaller* installer);
#endif

  base::WeakPtrFactory<PluginObserver> weak_ptr_factory_;

  TabContentsWrapper* tab_contents_;

#if defined(ENABLE_PLUGIN_INSTALLATION)
  ScopedVector<MissingPluginHost> missing_plugins_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PluginObserver);
};

#endif  // CHROME_BROWSER_PLUGIN_OBSERVER_H_
