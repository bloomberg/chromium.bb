// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_PROCESS_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_PROCESS_OBSERVER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/content_settings.h"
#include "content/public/renderer/render_process_observer.h"

class ChromeContentRendererClient;
class GURL;
struct ContentSettings;

namespace content {
class ResourceDispatcherDelegate;
}

// This class filters the incoming control messages (i.e. ones not destined for
// a RenderView) for Chrome specific messages that the content layer doesn't
// happen.  If a few messages are related, they should probably have their own
// observer.
class ChromeRenderProcessObserver : public content::RenderProcessObserver {
 public:
  explicit ChromeRenderProcessObserver(
      ChromeContentRendererClient* client);
  virtual ~ChromeRenderProcessObserver();

  static bool is_incognito_process() { return is_incognito_process_; }

  // Needs to be called by RenderViews in case of navigations to execute
  // any 'clear cache' commands that were delayed until the next navigation.
  void ExecutePendingClearCache();

  // Returns a pointer to the content setting rules owned by
  // |ChromeRenderProcessObserver|.
  const RendererContentSettingRules* content_setting_rules() const;

 private:
  // RenderProcessObserver implementation.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void WebKitInitialized() OVERRIDE;
  virtual void OnRenderProcessShutdown() OVERRIDE;

  void OnSetIsIncognitoProcess(bool is_incognito_process);
  void OnSetContentSettingsForCurrentURL(
      const GURL& url, const ContentSettings& content_settings);
  void OnSetContentSettingRules(const RendererContentSettingRules& rules);
  void OnSetCacheCapacities(size_t min_dead_capacity,
                            size_t max_dead_capacity,
                            size_t capacity);
  // If |on_navigation| is true, the clearing is delayed until the next
  // navigation event.
  void OnClearCache(bool on_navigation);
  void OnGetCacheResourceStats();
  void OnSetFieldTrialGroup(const std::string& fiel_trial_name,
                            const std::string& group_name);
  void OnGetV8HeapStats();

  static bool is_incognito_process_;
  scoped_ptr<content::ResourceDispatcherDelegate> resource_delegate_;
  ChromeContentRendererClient* client_;
  // If true, the web cache shall be cleared before the next navigation event.
  bool clear_cache_pending_;
  RendererContentSettingRules content_setting_rules_;

  bool webkit_initialized_;
  size_t pending_cache_min_dead_capacity_;
  size_t pending_cache_max_dead_capacity_;
  size_t pending_cache_capacity_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderProcessObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_PROCESS_OBSERVER_H_
