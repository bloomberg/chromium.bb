// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_placeholder.h"

#include "base/atomic_sequence_num.h"
#include "base/id_map.h"
#include "base/lazy_instance.h"
#include "base/process.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/values.h"
#include "content/common/view_messages.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_channel_handle.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPlugin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "webkit/plugins/webview_plugin.h"

static base::StaticAtomicSequenceNumber g_next_id;

// The global list of all Browser Plugin Placeholders within a process.
base::LazyInstance<IDMap<BrowserPluginPlaceholder> >::Leaky
    g_all_placeholders = LAZY_INSTANCE_INITIALIZER;

using webkit::WebViewPlugin;
using WebKit::WebPlugin;
using WebKit::WebPluginContainer;

const char* const kPluginPlaceholderDataURL =
    "about:blank";

// static
webkit::WebViewPlugin* BrowserPluginPlaceholder::Create(
    content::RenderView* render_view,
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params) {
  // TODO(fsamuel): Need a better loading screen at some point. Maybe this can
  // be a part of the <browser> API?
  // |browser_plugin| will destroy itself when its WebViewPlugin is going away.
  BrowserPluginPlaceholder* browser_plugin = new BrowserPluginPlaceholder(
      render_view, frame, params, "");
  return browser_plugin->plugin();
}

// static
BrowserPluginPlaceholder* BrowserPluginPlaceholder::FromID(int id) {
  return g_all_placeholders.Get().Lookup(id);
}

BrowserPluginPlaceholder::BrowserPluginPlaceholder(
    content::RenderView* render_view,
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params,
    const std::string& html_data)
    : render_view_(render_view),
      plugin_params_(params),
      plugin_(webkit::WebViewPlugin::Create(
              this, render_view->GetWebkitPreferences(), html_data,
                        GURL(kPluginPlaceholderDataURL))) {
  id_ = g_next_id.GetNext();
  RegisterPlaceholder(GetID(), this);

  // By default we navigate to google.com
  GetPluginParameters(0, 0, "http://www.google.com/");

  // TODO(fsamuel): Request a browser plugin instance from the
  // browser process.
}

BrowserPluginPlaceholder::~BrowserPluginPlaceholder() {
  UnregisterPlaceholder(GetID());
}

void BrowserPluginPlaceholder::RegisterPlaceholder(
    int id,
    BrowserPluginPlaceholder* placeholder) {
  g_all_placeholders.Get().AddWithID(placeholder, id);
}

void BrowserPluginPlaceholder::UnregisterPlaceholder(int id) {
  if (g_all_placeholders.Get().Lookup(id))
    g_all_placeholders.Get().Remove(id);
}

const WebKit::WebPluginParams& BrowserPluginPlaceholder::plugin_params() const {
    return plugin_params_;
}

void BrowserPluginPlaceholder::GetPluginParameters(
    int default_width, int default_height,
    const std::string& default_src) {
  int width = default_width;
  int height = default_height;

  // Get the plugin parameters from the attributes vector
  for (unsigned i = 0; i < plugin_params_.attributeNames.size(); ++i) {
    std::string attributeName = plugin_params_.attributeNames[i].utf8();
    if (LowerCaseEqualsASCII(attributeName, "width")) {
      std::string attributeValue = plugin_params_.attributeValues[i].utf8();
      CHECK(base::StringToInt(attributeValue, &width));
    } else if (LowerCaseEqualsASCII(attributeName, "height")) {
      std::string attributeValue = plugin_params_.attributeValues[i].utf8();
      CHECK(base::StringToInt(attributeValue, &height));
    } else if (LowerCaseEqualsASCII(attributeName, "src")) {
      src_ = plugin_params_.attributeValues[i].utf8();
    }
  }
  // If we didn't find the attributes set or they're not sensible,
  // we reset our attributes to the default.
  if (src_.empty())
    src_ = default_src;

  size_.SetSize(width, height);
}

void BrowserPluginPlaceholder::GuestReady(
    base::ProcessHandle process_handle,
    const IPC::ChannelHandle& channel_handle) {
  // TODO(fsamuel): Once the guest renderer is ready,
  // it will inform the host renderer and the BrowserPluginPlaceholder
  // can swap itself out with the guest.
  NOTIMPLEMENTED();
}

void BrowserPluginPlaceholder::LoadGuest(WebKit::WebPlugin* new_plugin) {
  WebKit::WebPluginContainer* container = plugin_->container();
  if (!new_plugin || !new_plugin->initialize(container))
    return;
  plugin_->RestoreTitleText();
  container->setPlugin(new_plugin);
  container->invalidate();
  container->reportGeometry();
  plugin_->ReplayReceivedData(new_plugin);
  plugin_->destroy();
}

void BrowserPluginPlaceholder::WillDestroyPlugin() {
  delete this;
}
