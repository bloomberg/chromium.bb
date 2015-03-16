// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUGINS_RENDERER_LOADABLE_PLUGIN_PLACEHOLDER_H_
#define COMPONENTS_PLUGINS_RENDERER_LOADABLE_PLUGIN_PLACEHOLDER_H_

#include "base/memory/weak_ptr.h"
#include "components/plugins/renderer/plugin_placeholder.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/plugin_instance_throttler.h"

namespace plugins {
// Placeholders can be used if a plugin is missing or not available
// (blocked or disabled).
class LoadablePluginPlaceholder
    : public PluginPlaceholder,
      public content::PluginInstanceThrottler::Observer {
 public:
  void set_blocked_for_background_tab(bool blocked_for_background_tab) {
    is_blocked_for_background_tab_ = blocked_for_background_tab;
  }

  void set_blocked_for_prerendering(bool blocked_for_prerendering) {
    is_blocked_for_prerendering_ = blocked_for_prerendering;
  }

#if defined(ENABLE_PLUGINS)
  void set_power_saver_enabled(bool power_saver_enabled) {
    power_saver_enabled_ = power_saver_enabled;
  }

  // Defer loading of plugin, and instead show the Power Saver poster image.
  void BlockForPowerSaverPoster();
#endif

  void set_allow_loading(bool allow_loading) { allow_loading_ = allow_loading; }

  // When we load the plugin, use this already-created plugin, not a new one.
  void SetPremadePlugin(content::PluginInstanceThrottler* throttler);

 protected:
  LoadablePluginPlaceholder(content::RenderFrame* render_frame,
                            blink::WebLocalFrame* frame,
                            const blink::WebPluginParams& params,
                            const std::string& html_data,
                            GURL placeholderDataUrl);

  ~LoadablePluginPlaceholder() override;

#if defined(ENABLE_PLUGINS)
  void MarkPluginEssential(
      content::PluginInstanceThrottler::PowerSaverUnthrottleMethod method);
#endif

  void OnLoadBlockedPlugins(const std::string& identifier);
  void OnSetIsPrerendering(bool is_prerendering);

  void SetMessage(const base::string16& message);
  void SetPluginInfo(const content::WebPluginInfo& plugin_info);
  const content::WebPluginInfo& GetPluginInfo() const;
  void SetIdentifier(const std::string& identifier);
  bool LoadingAllowed() const { return allow_loading_; }

  // Replace this placeholder with a different plugin (which could be
  // a placeholder again).
  void ReplacePlugin(blink::WebPlugin* new_plugin);

  // Hide this placeholder.
  void HidePlugin();

  // Load the blocked plugin.
  void LoadPlugin();

  // gin::Wrappable method:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

 private:
  // WebViewPlugin::Delegate methods:
  void PluginDestroyed() override;

  // RenderFrameObserver methods:
  void WasShown() override;

  // content::PluginInstanceThrottler::Observer methods:
  void OnThrottleStateChange() override;

  // Javascript callbacks:

  // Load the blocked plugin by calling LoadPlugin().
  void LoadCallback();

  // Hide the blocked plugin by calling HidePlugin().
  void HideCallback();

  void DidFinishLoadingCallback();

  void UpdateMessage();

  bool LoadingBlocked() const;

  content::WebPluginInfo plugin_info_;

  base::string16 message_;

  // True if the plugin load was deferred due to page being a background tab.
  // Plugin may be automatically loaded when the page is foregrounded.
  bool is_blocked_for_background_tab_;

  // True if the plugin was blocked because the page was being prerendered.
  // Plugin may be automatically be loaded when the page is displayed.
  bool is_blocked_for_prerendering_;

  // True if the plugin load was deferred due to a Power Saver poster.
  bool is_blocked_for_power_saver_poster_;

  // This is independent of deferred plugin load due to a Power Saver poster.
  bool power_saver_enabled_;

  // True if the plugin has been marked essential.
  bool plugin_marked_essential_;

  // When we load, uses this premade plugin instead of creating a new one.
  content::PluginInstanceThrottler* premade_throttler_;

  bool allow_loading_;

  // True if the placeholder was replaced with the real plugin.
  bool placeholder_was_replaced_;

  bool hidden_;
  bool finished_loading_;
  std::string identifier_;

  base::WeakPtrFactory<LoadablePluginPlaceholder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LoadablePluginPlaceholder);
};

}  // namespace plugins

#endif  // COMPONENTS_PLUGINS_RENDERER_LOADABLE_PLUGIN_PLACEHOLDER_H_
