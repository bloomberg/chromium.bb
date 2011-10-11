// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_PROCESS_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_PROCESS_OBSERVER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_process_observer.h"

namespace chrome {
class ChromeContentRendererClient;
}

class GURL;
class ResourceDispatcherDelegate;
struct ContentSettings;

// This class filters the incoming control messages (i.e. ones not destined for
// a RenderView) for Chrome specific messages that the content layer doesn't
// happen.  If a few messages are related, they should probably have their own
// observer.
class ChromeRenderProcessObserver : public content::RenderProcessObserver {
 public:
  explicit ChromeRenderProcessObserver(
      chrome::ChromeContentRendererClient* client);
  virtual ~ChromeRenderProcessObserver();

  static bool is_incognito_process() { return is_incognito_process_; }

  // Needs to be called by RenderViews in case of navigations to execute
  // any 'clear cache' commands that were delayed until the next navigation.
  void ExecutePendingClearCache();

 private:
  // RenderProcessObserver implementation.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void WebKitInitialized() OVERRIDE;

  void OnSetIsIncognitoProcess(bool is_incognito_process);
  void OnSetContentSettingsForCurrentURL(
      const GURL& url, const ContentSettings& content_settings);
  void OnSetDefaultContentSettings(const ContentSettings& content_settings);
  void OnSetCacheCapacities(size_t min_dead_capacity,
                            size_t max_dead_capacity,
                            size_t capacity);
  // If |on_navigation| is true, the clearing is delayed until the next
  // navigation event.
  void OnClearCache(bool on_navigation);
  void OnGetCacheResourceStats();
  void OnSetFieldTrialGroup(const std::string& fiel_trial_name,
                            const std::string& group_name);
  void OnGetRendererTcmalloc();
  void OnSetTcmallocHeapProfiling(bool profiling, const std::string& prefix);
  void OnWriteTcmallocHeapProfile(const FilePath::StringType& filename);
  void OnGetV8HeapStats();
  void OnPurgeMemory();

  static bool is_incognito_process_;
  scoped_ptr<ResourceDispatcherDelegate> resource_delegate_;
  chrome::ChromeContentRendererClient* client_;
  // If true, the web cache shall be cleared before the next navigation event.
  bool clear_cache_pending_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderProcessObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_PROCESS_OBSERVER_H_
