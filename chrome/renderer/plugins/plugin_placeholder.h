// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PLUGINS_PLUGIN_PLACEHOLDER_H_
#define CHROME_RENDERER_PLUGINS_PLUGIN_PLACEHOLDER_H_
#pragma once

#include "content/public/renderer/render_process_observer.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "webkit/glue/cpp_bound_class.h"
#include "webkit/plugins/webplugininfo.h"
#include "webkit/plugins/webview_plugin.h"

struct ChromeViewHostMsg_GetPluginInfo_Status;

namespace webkit {
namespace npapi {
class PluginGroup;
}
struct WebPluginInfo;
}

// Placeholders can be used if a plug-in is missing or not available
// (blocked or disabled).
class PluginPlaceholder : public content::RenderViewObserver,
                          public content::RenderProcessObserver,
                          public CppBoundClass,
                          public webkit::WebViewPlugin::Delegate {
 public:
  // Creates a new WebViewPlugin with a MissingPlugin as a delegate.
  static PluginPlaceholder* CreateMissingPlugin(
      content::RenderView* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params);

  static PluginPlaceholder* CreateErrorPlugin(
      content::RenderView* render_view,
      const FilePath& plugin_path);

  static PluginPlaceholder* CreateBlockedPlugin(
      content::RenderView* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params,
      const webkit::WebPluginInfo& info,
      const string16& name,
      int resource_id,
      int message_id);

#if defined(ENABLE_MOBILE_YOUTUBE_PLUGIN)
  // Placeholder for old style embedded youtube video on mobile device. For old
  // style embedded youtube video, it has a url in the form of
  // http://www.youtube.com/v/VIDEO_ID. This placeholder replaces the url with a
  // simple html page and clicking the play image redirects the user to the
  // mobile youtube app.
  static PluginPlaceholder* CreateMobileYoutubePlugin(
       content::RenderView* render_view,
       WebKit::WebFrame* frame,
       const WebKit::WebPluginParams& params);
#endif

  webkit::WebViewPlugin* plugin() { return plugin_; }

  void set_blocked_for_prerendering(bool blocked_for_prerendering) {
    is_blocked_for_prerendering_ = blocked_for_prerendering;
  }

  void set_allow_loading(bool allow_loading) { allow_loading_ = allow_loading; }

  void SetStatus(const ChromeViewHostMsg_GetPluginInfo_Status& status);

#if defined(ENABLE_PLUGIN_INSTALLATION)
  int32 CreateRoutingId();
#endif

#if defined(ENABLE_MOBILE_YOUTUBE_PLUGIN)
  // Whether this is a youtube url.
  static bool IsYouTubeURL(const GURL& url, const std::string& mime_type);
#endif

 private:
  // |render_view| and |frame| are weak pointers. If either one is going away,
  // our |plugin_| will be destroyed as well and will notify us.
  PluginPlaceholder(content::RenderView* render_view,
                    WebKit::WebFrame* frame,
                    const WebKit::WebPluginParams& params,
                    const std::string& html_data,
                    const string16& title);

  virtual ~PluginPlaceholder();

  // WebViewPlugin::Delegate methods:
  virtual void BindWebFrame(WebKit::WebFrame* frame) OVERRIDE;
  virtual void WillDestroyPlugin() OVERRIDE;
  virtual void ShowContextMenu(const WebKit::WebMouseEvent&) OVERRIDE;

  // content::RenderViewObserver methods:
  virtual void ContextMenuAction(unsigned id) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // content::RenderProcessObserver methods:
  virtual void PluginListChanged() OVERRIDE;

  // Replace this placeholder with a different plugin (which could be
  // a placeholder again).
  void ReplacePlugin(WebKit::WebPlugin* new_plugin);

  // Hide this placeholder.
  void HidePlugin();

  // Load the blocked plugin.
  void LoadPlugin();

  // Javascript callbacks:
  // Load the blocked plugin by calling LoadPlugin().
  // Takes no arguments, and returns nothing.
  void LoadCallback(const CppArgumentList& args, CppVariant* result);

  // Hide the blocked plugin by calling HidePlugin().
  // Takes no arguments, and returns nothing.
  void HideCallback(const CppArgumentList& args, CppVariant* result);

  // Opens chrome://plugins in a new tab.
  // Takes no arguments, and returns nothing.
  void OpenAboutPluginsCallback(const CppArgumentList& args,
                                CppVariant* result);

  void DidFinishLoadingCallback(const CppArgumentList& args,
                                CppVariant* result);

  void OnLoadBlockedPlugins();
  void OnSetIsPrerendering(bool is_prerendering);
  void OnDidNotFindMissingPlugin();
#if defined(ENABLE_PLUGIN_INSTALLATION)
  void OnFoundMissingPlugin(const string16& plugin_name);
  void OnStartedDownloadingPlugin();
  void OnFinishedDownloadingPlugin();
  void OnErrorDownloadingPlugin(const std::string& error);
  void OnCancelledDownloadingPlugin();
#endif

#if defined(ENABLE_MOBILE_YOUTUBE_PLUGIN)
 // Check whether the url is valid.
  static bool IsValidYouTubeVideo(const std::string& path);

  // Opens a youtube app in the current tab.
  void OpenYoutubeUrlCallback(const CppArgumentList& args, CppVariant* result);
#endif

  void SetMessage(const string16& message);
  void UpdateMessage();

  WebKit::WebFrame* frame_;
  WebKit::WebPluginParams plugin_params_;
  webkit::WebViewPlugin* plugin_;

  webkit::WebPluginInfo plugin_info_;

  string16 title_;
  string16 message_;

  // We use a scoped_ptr so we can forward-declare the struct; it's defined in
  // an IPC message file which can't be easily included in other header files.
  scoped_ptr<ChromeViewHostMsg_GetPluginInfo_Status> status_;

  // True iff the plugin was blocked because the page was being prerendered.
  // Plugin will automatically be loaded when the page is displayed.
  bool is_blocked_for_prerendering_;
  bool allow_loading_;

#if defined(ENABLE_PLUGIN_INSTALLATION)
  // |routing_id()| is the routing ID of our associated RenderView, but we have
  // a separate routing ID for messages specific to this placeholder.
  int32 placeholder_routing_id_;
#endif

  bool hidden_;
  bool has_host_;
  bool finished_loading_;
  string16 plugin_name_;

  DISALLOW_COPY_AND_ASSIGN(PluginPlaceholder);
};

#endif  // CHROME_RENDERER_PLUGINS_PLUGIN_PLACEHOLDER_H_
