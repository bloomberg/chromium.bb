// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUGINS_RENDERER_PLUGIN_PLACEHOLDER_H_
#define COMPONENTS_PLUGINS_RENDERER_PLUGIN_PLACEHOLDER_H_

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/plugins/renderer/webview_plugin.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/context_menu_client.h"
#include "content/public/renderer/render_process_observer.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "v8/include/v8.h"

namespace content {
struct WebPluginInfo;
}

namespace plugins {
// Placeholders can be used if a plug-in is missing or not available
// (blocked or disabled).
class PluginPlaceholder : public content::RenderViewObserver,
                          public WebViewPlugin::Delegate {
 public:

  WebViewPlugin* plugin() { return plugin_; }

  void set_blocked_for_prerendering(bool blocked_for_prerendering) {
    is_blocked_for_prerendering_ = blocked_for_prerendering;
  }

  void set_allow_loading(bool allow_loading) { allow_loading_ = allow_loading; }

 protected:
  // |render_view| and |frame| are weak pointers. If either one is going away,
  // our |plugin_| will be destroyed as well and will notify us.
  PluginPlaceholder(content::RenderView* render_view,
                    blink::WebFrame* frame,
                    const blink::WebPluginParams& params,
                    const std::string& html_data,
                    GURL placeholderDataUrl);

  virtual ~PluginPlaceholder();

  // Derived classes should invoke this method to install additional callbacks
  // on the window.plugin object. When a callback is invoked, it will be
  // forwarded to |callback|.
  void RegisterCallback(const std::string& callback_name,
                        const base::Closure& callback);

  void OnLoadBlockedPlugins(const std::string& identifier);
  void OnSetIsPrerendering(bool is_prerendering);

  void SetMessage(const string16& message);
  void SetPluginInfo(const content::WebPluginInfo& plugin_info);
  const content::WebPluginInfo& GetPluginInfo() const;
  void SetIdentifier(const std::string& identifier);
  blink::WebFrame* GetFrame();
  const blink::WebPluginParams& GetPluginParams() const;
  bool LoadingAllowed() const { return allow_loading_; }

  // Replace this placeholder with a different plugin (which could be
  // a placeholder again).
  void ReplacePlugin(blink::WebPlugin* new_plugin);

  // Hide this placeholder.
  void HidePlugin();

  // Load the blocked plugin.
  void LoadPlugin();

  // WebViewPlugin::Delegate method:
  virtual void BindWebFrame(blink::WebFrame* frame) OVERRIDE;

 private:
  // WebViewPlugin::Delegate methods:
  virtual void WillDestroyPlugin() OVERRIDE;
  virtual void ShowContextMenu(const blink::WebMouseEvent&) OVERRIDE;

  // Javascript callbacks:

  // Load the blocked plugin by calling LoadPlugin().
  void LoadCallback();

  // Hide the blocked plugin by calling HidePlugin().
  void HideCallback();

  void DidFinishLoadingCallback();

  void UpdateMessage();

  blink::WebFrame* frame_;
  blink::WebPluginParams plugin_params_;
  WebViewPlugin* plugin_;

  content::WebPluginInfo plugin_info_;

  string16 message_;

  // True iff the plugin was blocked because the page was being prerendered.
  // Plugin will automatically be loaded when the page is displayed.
  bool is_blocked_for_prerendering_;
  bool allow_loading_;

  bool hidden_;
  bool finished_loading_;
  std::string identifier_;

  std::map<std::string, base::Closure> callbacks_;

  base::WeakPtrFactory<PluginPlaceholder> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginPlaceholder);
};

}  // namespace plugins

#endif  // COMPONENTS_PLUGINS_RENDERER_PLUGIN_PLACEHOLDER_H_
