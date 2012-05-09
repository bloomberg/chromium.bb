// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CONTENT_SETTINGS_OBSERVER_H_
#define CHROME_RENDERER_CONTENT_SETTINGS_OBSERVER_H_
#pragma once

#include <map>
#include <set>

#include "chrome/common/content_settings.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"

class GURL;

namespace WebKit {
class WebFrame;
class WebSecurityOrigin;
class WebURL;
}

// Handles blocking content per content settings for each RenderView.
class ContentSettingsObserver
    : public content::RenderViewObserver,
      public content::RenderViewObserverTracker<ContentSettingsObserver> {
 public:
  explicit ContentSettingsObserver(content::RenderView* render_view);
  virtual ~ContentSettingsObserver();

  // Sets the content setting rules which back |AllowImage()|, |AllowScript()|,
  // and |AllowScriptFromSource()|. |content_setting_rules| must outlive this
  // |ContentSettingsObserver|.
  void SetContentSettingRules(
      const RendererContentSettingRules* content_setting_rules);

  bool IsPluginTemporarilyAllowed(const std::string& identifier);

  // Sends an IPC notification that the specified content type was blocked.
  // If the content type requires it, |resource_identifier| names the specific
  // resource that was blocked (the plugin path in the case of plugins),
  // otherwise it's the empty string.
  void DidBlockContentType(ContentSettingsType settings_type,
                           const std::string& resource_identifier);

  // These correspond to WebKit::WebPermissionClient methods.
  bool AllowDatabase(WebKit::WebFrame* frame,
                     const WebKit::WebString& name,
                     const WebKit::WebString& display_name,
                     unsigned long estimated_size);
  bool AllowFileSystem(WebKit::WebFrame* frame);
  bool AllowImage(WebKit::WebFrame* frame,
                  bool enabled_per_settings,
                  const WebKit::WebURL& image_url);
  bool AllowIndexedDB(WebKit::WebFrame* frame,
                      const WebKit::WebString& name,
                      const WebKit::WebSecurityOrigin& origin);
  bool AllowPlugins(WebKit::WebFrame* frame, bool enabled_per_settings);
  bool AllowScript(WebKit::WebFrame* frame, bool enabled_per_settings);
  bool AllowScriptFromSource(WebKit::WebFrame* frame, bool enabled_per_settings,
                             const WebKit::WebURL& script_url);
  bool AllowStorage(WebKit::WebFrame* frame, bool local);
  void DidNotAllowPlugins(WebKit::WebFrame* frame);
  void DidNotAllowScript(WebKit::WebFrame* frame);

 private:
  FRIEND_TEST_ALL_PREFIXES(ContentSettingsObserverTest, WhitelistedSchemes);
  FRIEND_TEST_ALL_PREFIXES(ChromeRenderViewTest,
                           ContentSettingsInterstitialPages);

  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidCommitProvisionalLoad(WebKit::WebFrame* frame,
                                        bool is_new_navigation) OVERRIDE;

  // Message handlers.
  void OnLoadBlockedPlugins(const std::string& identifier);
  void OnSetAsInterstitial();

  // Resets the |content_blocked_| array.
  void ClearBlockedContentSettings();

  // Helpers.
  // True if |frame| contains content that is white-listed for content settings.
  static bool IsWhitelistedForContentSettings(WebKit::WebFrame* frame);
  static bool IsWhitelistedForContentSettings(
      const WebKit::WebSecurityOrigin& origin,
      const GURL& document_url);

  // A pointer to content setting rules stored by the renderer. Normally, the
  // |RendererContentSettingRules| object is owned by
  // |ChromeRenderProcessObserver|. In the tests it is owned by the caller of
  // |SetContentSettingRules|.
  const RendererContentSettingRules* content_setting_rules_;

  // Stores if images, scripts, and plugins have actually been blocked.
  bool content_blocked_[CONTENT_SETTINGS_NUM_TYPES];

  // Caches the result of AllowStorage.
  typedef std::pair<GURL, bool> StoragePermissionsKey;
  std::map<StoragePermissionsKey, bool> cached_storage_permissions_;

  // Caches the result of |AllowScript|.
  std::map<WebKit::WebFrame*, bool> cached_script_permissions_;

  std::set<std::string> temporarily_allowed_plugins_;
  bool is_interstitial_page_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingsObserver);
};

#endif  // CHROME_RENDERER_CONTENT_SETTINGS_OBSERVER_H_
