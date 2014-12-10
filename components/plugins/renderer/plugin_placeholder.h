// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUGINS_RENDERER_PLUGIN_PLACEHOLDER_H_
#define COMPONENTS_PLUGINS_RENDERER_PLUGIN_PLACEHOLDER_H_

#include "base/memory/weak_ptr.h"
#include "components/plugins/renderer/webview_plugin.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/context_menu_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_process_observer.h"
#include "gin/wrappable.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"

namespace content {
struct WebPluginInfo;
}

namespace plugins {
// Placeholders can be used if a plug-in is missing or not available
// (blocked or disabled).
class PluginPlaceholder : public content::RenderFrameObserver,
                          public WebViewPlugin::Delegate,
                          public gin::Wrappable<PluginPlaceholder> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  WebViewPlugin* plugin() { return plugin_; }

  void set_blocked_for_prerendering(bool blocked_for_prerendering) {
    is_blocked_for_prerendering_ = blocked_for_prerendering;
  }

#if defined(ENABLE_PLUGINS)
  void set_power_saver_mode(
      content::RenderFrame::PluginPowerSaverMode power_saver_mode) {
    power_saver_mode_ = power_saver_mode;
  }

  // Defer loading of plug-in, and instead show the Power Saver poster image.
  void BlockForPowerSaverPoster();
#endif

  void set_allow_loading(bool allow_loading) { allow_loading_ = allow_loading; }

 protected:
  // |render_frame| and |frame| are weak pointers. If either one is going away,
  // our |plugin_| will be destroyed as well and will notify us.
  PluginPlaceholder(content::RenderFrame* render_frame,
                    blink::WebLocalFrame* frame,
                    const blink::WebPluginParams& params,
                    const std::string& html_data,
                    GURL placeholderDataUrl);

  ~PluginPlaceholder() override;

#if defined(ENABLE_PLUGINS)
  void DisablePowerSaverForInstance();
#endif

  void OnLoadBlockedPlugins(const std::string& identifier);
  void OnSetIsPrerendering(bool is_prerendering);

  void SetMessage(const base::string16& message);
  void SetPluginInfo(const content::WebPluginInfo& plugin_info);
  const content::WebPluginInfo& GetPluginInfo() const;
  void SetIdentifier(const std::string& identifier);
  blink::WebLocalFrame* GetFrame();
  const blink::WebPluginParams& GetPluginParams() const;
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
  void ShowContextMenu(const blink::WebMouseEvent&) override;
  void PluginDestroyed() override;

  // RenderFrameObserver methods:
  void OnDestruct() override;

  // Javascript callbacks:

  // Load the blocked plugin by calling LoadPlugin().
  void LoadCallback();

  // Hide the blocked plugin by calling HidePlugin().
  void HideCallback();

  void DidFinishLoadingCallback();

  void UpdateMessage();

  blink::WebLocalFrame* frame_;
  blink::WebPluginParams plugin_params_;
  WebViewPlugin* plugin_;

  content::WebPluginInfo plugin_info_;

  base::string16 message_;

  // True if the plugin was blocked because the page was being prerendered.
  // Plugin may be automatically be loaded when the page is displayed.
  bool is_blocked_for_prerendering_;

  // True if the plugin load is deferred due to a Power Saver poster.
  bool is_blocked_for_power_saver_poster_;

  // This is independent of deferred plugin load due to a Power Saver poster.
  content::RenderFrame::PluginPowerSaverMode power_saver_mode_;

  bool allow_loading_;

  bool hidden_;
  bool finished_loading_;
  std::string identifier_;

  base::WeakPtrFactory<PluginPlaceholder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginPlaceholder);
};

}  // namespace plugins

#endif  // COMPONENTS_PLUGINS_RENDERER_PLUGIN_PLACEHOLDER_H_
