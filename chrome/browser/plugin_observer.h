// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_OBSERVER_H_
#define CHROME_BROWSER_PLUGIN_OBSERVER_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include <map>
#endif

class GURL;
class InfoBarDelegate;
class PluginFinder;
class TabContentsWrapper;

#if defined(ENABLE_PLUGIN_INSTALLATION)
class PluginInstaller;
class PluginPlaceholderHost;
#endif

class PluginObserver : public content::WebContentsObserver {
 public:
  explicit PluginObserver(TabContentsWrapper* tab_contents);
  virtual ~PluginObserver();

  // content::WebContentsObserver implementation.
  virtual void PluginCrashed(const FilePath& plugin_path) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

#if defined(ENABLE_PLUGIN_INSTALLATION)
  void InstallMissingPlugin(PluginInstaller* installer);
#endif

  TabContentsWrapper* tab_contents_wrapper() { return tab_contents_; }

 private:
  class PluginPlaceholderHost;

  void OnBlockedUnauthorizedPlugin(const string16& name,
                                   const std::string& identifier);
  void OnBlockedOutdatedPlugin(int placeholder_id,
                               const std::string& identifier);
#if defined(ENABLE_PLUGIN_INSTALLATION)
  void OnFindMissingPlugin(int placeholder_id, const std::string& mime_type);

  void FindMissingPlugin(int placeholder_id,
                         const std::string& mime_type,
                         PluginFinder* plugin_finder);
  void FindPluginToUpdate(int placeholder_id,
                          const std::string& identifier,
                          PluginFinder* plugin_finder);
  void OnRemovePluginPlaceholderHost(int placeholder_id);
#endif
  void OnOpenAboutPlugins();

  base::WeakPtrFactory<PluginObserver> weak_ptr_factory_;

  TabContentsWrapper* tab_contents_;

#if defined(ENABLE_PLUGIN_INSTALLATION)
  // Stores all PluginPlaceholderHosts, keyed by their routing ID.
  std::map<int, PluginPlaceholderHost*> plugin_placeholders_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PluginObserver);
};

#endif  // CHROME_BROWSER_PLUGIN_OBSERVER_H_
