// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/old/browser_plugin.h"

#include "base/atomic_sequence_num.h"
#include "base/id_map.h"
#include "base/lazy_instance.h"
#include "base/process.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/values.h"
#include "content/common/browser_plugin_messages.h"
#include "content/public/common/url_constants.h"
#include "content/renderer/render_view_impl.h"
#include "ipc/ipc_channel_handle.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPlugin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppapi_webplugin_impl.h"
#include "webkit/plugins/webview_plugin.h"

static int g_next_id = 0;

// The global list of all Browser Plugin Placeholders within a process.
base::LazyInstance<IDMap<BrowserPlugin> >::Leaky
    g_all_browser_plugins = LAZY_INSTANCE_INITIALIZER;

using WebKit::WebPlugin;
using WebKit::WebPluginContainer;
using webkit::WebViewPlugin;

void Register(int id, BrowserPlugin* browser_plugin) {
  g_all_browser_plugins.Get().AddWithID(browser_plugin, id);
}

void Unregister(int id) {
  if (g_all_browser_plugins.Get().Lookup(id))
    g_all_browser_plugins.Get().Remove(id);
}

// static
WebKit::WebPlugin* BrowserPlugin::Create(
    RenderViewImpl* render_view,
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params) {
  // TODO(fsamuel): Figure out what the lifetime is of this class.
  // It seems we need to blow away this object once a WebPluginContainer is
  // gone. How do we detect it's gone? A WebKit change perhaps?
  BrowserPlugin* browser_plugin = new BrowserPlugin(
      render_view, frame, params, "");
  return browser_plugin->placeholder();
}

// static
BrowserPlugin* BrowserPlugin::FromID(int id) {
  return g_all_browser_plugins.Get().Lookup(id);
}

BrowserPlugin::BrowserPlugin(
    RenderViewImpl* render_view,
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params,
    const std::string& html_data)
    : render_view_(render_view),
      plugin_params_(params),
      placeholder_(webkit::WebViewPlugin::Create(
                       NULL,
                       render_view->GetWebkitPreferences(),
                       html_data,
                       GURL(chrome::kAboutBlankURL))),
      plugin_(NULL) {
  id_ = ++g_next_id;
  Register(id_, this);

  // By default we do not navigate and simply stay with an
  // about:blank placeholder.
  std::string src;
  ParseSrcAttribute("", &src);

  if (!src.empty()) {
    render_view->Send(new BrowserPluginHostMsg_NavigateFromEmbedder(
        render_view->GetRoutingID(),
        id_,
        frame->identifier(),
        src));
  }
}

BrowserPlugin::~BrowserPlugin() {
  Unregister(id_);
}

void BrowserPlugin::ParseSrcAttribute(
    const std::string& default_src,
    std::string* src) {
  // Get the src attribute from the attributes vector
  for (unsigned i = 0; i < plugin_params_.attributeNames.size(); ++i) {
    std::string attributeName = plugin_params_.attributeNames[i].utf8();
    if (LowerCaseEqualsASCII(attributeName, "src")) {
      *src = plugin_params_.attributeValues[i].utf8();
      break;
    }
  }
  // If we didn't find the attributes set or they're not sensible,
  // we reset our attributes to the default.
  if (src->empty()) {
    *src = default_src;
  }
}

void BrowserPlugin::LoadGuest(
    int guest_process_id,
    const IPC::ChannelHandle& channel_handle) {
  webkit::ppapi::WebPluginImpl* new_guest =
      render_view()->CreateBrowserPlugin(channel_handle,
                                         guest_process_id,
                                         plugin_params());
  Replace(new_guest);
}

void BrowserPlugin::AdvanceFocus(bool reverse) {
  // TODO(fsamuel): Uncomment this once http://wkbug.com/88827 lands.
  // render_view()->GetWebView()->advanceFocus(reverse);
}

void BrowserPlugin::Replace(
    webkit::ppapi::WebPluginImpl* new_plugin) {
  WebKit::WebPlugin* current_plugin =
      plugin_ ? static_cast<WebKit::WebPlugin*>(plugin_) : placeholder_;
  WebKit::WebPluginContainer* container = current_plugin->container();
  if (!new_plugin || !new_plugin->initialize(container))
    return;

  // Clear the container's backing texture ID.
  if (plugin_)
    plugin_->instance()->BindGraphics(plugin_->instance()->pp_instance(), 0);

  PP_Instance instance = new_plugin->instance()->pp_instance();
  ppapi::proxy::HostDispatcher* dispatcher =
      ppapi::proxy::HostDispatcher::GetForInstance(instance);
  dispatcher->Send(new BrowserPluginMsg_GuestReady(instance, id_));

  // TODO(fsamuel): We should delay the swapping out of the current plugin
  // until after the guest's WebGraphicsContext3D has been initialized. That
  // way, we immediately have something to render onto the screen.
  container->setPlugin(new_plugin);
  container->invalidate();
  container->reportGeometry();
  if (plugin_)
    plugin_->destroy();
  plugin_ = new_plugin;
}
