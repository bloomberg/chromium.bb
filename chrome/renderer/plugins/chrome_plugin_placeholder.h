// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PLUGINS_CHROME_PLUGIN_PLACEHOLDER_H_
#define CHROME_RENDERER_PLUGINS_CHROME_PLUGIN_PLACEHOLDER_H_

#include "components/plugins/renderer/plugin_placeholder.h"

struct ChromeViewHostMsg_GetPluginInfo_Status;

class ChromePluginPlaceholder : public plugins::PluginPlaceholder,
                                public content::RenderProcessObserver,
                                public content::ContextMenuClient {
 public:
  static const char kPluginPlaceholderDataURL[];

  static ChromePluginPlaceholder* CreateBlockedPlugin(
      content::RenderFrame* render_frame,
      blink::WebLocalFrame* frame,
      const blink::WebPluginParams& params,
      const content::WebPluginInfo& info,
      const std::string& identifier,
      const base::string16& name,
      int resource_id,
      const base::string16& message);

  // Creates a new WebViewPlugin with a MissingPlugin as a delegate.
  static ChromePluginPlaceholder* CreateMissingPlugin(
      content::RenderFrame* render_frame,
      blink::WebLocalFrame* frame,
      const blink::WebPluginParams& params);

  static ChromePluginPlaceholder* CreateErrorPlugin(
      content::RenderFrame* render_frame,
      const base::FilePath& plugin_path);

  void SetStatus(const ChromeViewHostMsg_GetPluginInfo_Status& status);

#if defined(ENABLE_PLUGIN_INSTALLATION)
  int32 CreateRoutingId();
#endif

 private:
  ChromePluginPlaceholder(content::RenderFrame* render_frame,
                          blink::WebLocalFrame* frame,
                          const blink::WebPluginParams& params,
                          const std::string& html_data,
                          const base::string16& title);
  virtual ~ChromePluginPlaceholder();

  // WebViewPlugin::Delegate (via PluginPlaceholder) method
  virtual void BindWebFrame(blink::WebFrame* frame) OVERRIDE;

  // gin::Wrappable (via PluginPlaceholder) method
  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE;

  // content::RenderViewObserver (via PluginPlaceholder) override:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WebViewPlugin::Delegate (via PluginPlaceholder) methods:
  virtual void ShowContextMenu(const blink::WebMouseEvent&) OVERRIDE;

  // content::RenderProcessObserver methods:
  virtual void PluginListChanged() OVERRIDE;

  // content::ContextMenuClient methods:
  virtual void OnMenuAction(int request_id, unsigned action) OVERRIDE;
  virtual void OnMenuClosed(int request_id) OVERRIDE;

  // Javascript callback opens chrome://plugins in a new tab.
  void OpenAboutPluginsCallback();

  void OnLoadBlockedPlugins(const std::string& identifier);
  void OnSetIsPrerendering(bool is_prerendering);
#if defined(ENABLE_PLUGIN_INSTALLATION)
  void OnDidNotFindMissingPlugin();
  void OnFoundMissingPlugin(const base::string16& plugin_name);
  void OnStartedDownloadingPlugin();
  void OnFinishedDownloadingPlugin();
  void OnErrorDownloadingPlugin(const std::string& error);
  void OnCancelledDownloadingPlugin();
#endif

  // We use a scoped_ptr so we can forward-declare the struct; it's defined in
  // an IPC message file which can't be easily included in other header files.
  scoped_ptr<ChromeViewHostMsg_GetPluginInfo_Status> status_;

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
