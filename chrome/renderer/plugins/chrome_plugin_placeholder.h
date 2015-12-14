// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PLUGINS_CHROME_PLUGIN_PLACEHOLDER_H_
#define CHROME_RENDERER_PLUGINS_CHROME_PLUGIN_PLACEHOLDER_H_

#include "components/plugins/renderer/loadable_plugin_placeholder.h"
#include "content/public/renderer/context_menu_client.h"
#include "content/public/renderer/render_process_observer.h"

namespace gfx {
class Size;
}

enum class ChromeViewHostMsg_GetPluginInfo_Status;

// This contains information specifying the poster image of plugin placeholders.
// The default constructor specifies no poster image.
struct PlaceholderPosterInfo {
  // The poster image specified in image 'srcset' attribute format.
  std::string poster_attribute;

  // Used to resolve relative paths in |poster_attribute|.
  GURL base_url;

  // Specify this to provide partially obscured plugins a centered poster image.
  gfx::Size custom_poster_size;
};

class ChromePluginPlaceholder final
    : public plugins::LoadablePluginPlaceholder,
      public content::RenderProcessObserver,
      public content::ContextMenuClient,
      public gin::Wrappable<ChromePluginPlaceholder> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static ChromePluginPlaceholder* CreateBlockedPlugin(
      content::RenderFrame* render_frame,
      blink::WebLocalFrame* frame,
      const blink::WebPluginParams& params,
      const content::WebPluginInfo& info,
      const std::string& identifier,
      const base::string16& name,
      int resource_id,
      const base::string16& message,
      const PlaceholderPosterInfo& poster_info);

  // Creates a new WebViewPlugin with a MissingPlugin as a delegate.
  static ChromePluginPlaceholder* CreateLoadableMissingPlugin(
      content::RenderFrame* render_frame,
      blink::WebLocalFrame* frame,
      const blink::WebPluginParams& params);

  void SetStatus(ChromeViewHostMsg_GetPluginInfo_Status status);

#if defined(ENABLE_PLUGIN_INSTALLATION)
  int32 CreateRoutingId();
#endif

 private:
  ChromePluginPlaceholder(content::RenderFrame* render_frame,
                          blink::WebLocalFrame* frame,
                          const blink::WebPluginParams& params,
                          const std::string& html_data,
                          const base::string16& title);
  ~ChromePluginPlaceholder() override;

  // content::LoadablePluginPlaceholder method
  blink::WebPlugin* CreatePlugin() override;

  // gin::Wrappable (via PluginPlaceholder) method
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  // content::RenderViewObserver (via PluginPlaceholder) override:
  bool OnMessageReceived(const IPC::Message& message) override;

  // WebViewPlugin::Delegate (via PluginPlaceholder) methods:
  v8::Local<v8::Value> GetV8Handle(v8::Isolate* isolate) override;
  void ShowContextMenu(const blink::WebMouseEvent&) override;

  // content::RenderProcessObserver methods:
  void PluginListChanged() override;

  // content::ContextMenuClient methods:
  void OnMenuAction(int request_id, unsigned action) override;
  void OnMenuClosed(int request_id) override;

  // Javascript callbacks:
  // Open chrome://plugins in a new tab.
  void OpenAboutPluginsCallback();

  // IPC message handlers:
#if defined(ENABLE_PLUGIN_INSTALLATION)
  void OnDidNotFindMissingPlugin();
  void OnFoundMissingPlugin(const base::string16& plugin_name);
  void OnStartedDownloadingPlugin();
  void OnFinishedDownloadingPlugin();
  void OnErrorDownloadingPlugin(const std::string& error);
  void OnCancelledDownloadingPlugin();
#endif

  ChromeViewHostMsg_GetPluginInfo_Status status_;

  base::string16 title_;

#if defined(ENABLE_PLUGIN_INSTALLATION)
  // |routing_id()| is the routing ID of our associated RenderView, but we have
  // a separate routing ID for messages specific to this placeholder.
  int32 placeholder_routing_id_;
#endif

  bool has_host_;
  int context_menu_request_id_;  // Nonzero when request pending.
  base::string16 plugin_name_;

  DISALLOW_COPY_AND_ASSIGN(ChromePluginPlaceholder);
};

#endif  // CHROME_RENDERER_PLUGINS_CHROME_PLUGIN_PLACEHOLDER_H_
