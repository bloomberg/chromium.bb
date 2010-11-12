// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/blocked_plugin.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/plugins/plugin_group.h"
#include "webkit/glue/plugins/webview_plugin.h"
#include "webkit/glue/webpreferences.h"

using WebKit::WebFrame;
using WebKit::WebPlugin;
using WebKit::WebPluginContainer;
using WebKit::WebPluginParams;

static const char* const kBlockedPluginDataURL = "chrome://blockedplugindata/";

BlockedPlugin::BlockedPlugin(RenderView* render_view,
                             WebFrame* frame,
                             const WebPluginParams& params,
                             const WebPreferences& preferences,
                             int template_id,
                             const string16& message)
    : render_view_(render_view),
      frame_(frame),
      plugin_params_(params) {
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(template_id));

  DCHECK(!template_html.empty()) << "unable to load template. ID: "
                                 << template_id;

  DictionaryValue values;
  values.SetString("message", message);

  // "t" is the id of the templates root node.
  std::string html_data = jstemplate_builder::GetTemplatesHtml(
      template_html, &values, "t");

  plugin_ = WebViewPlugin::Create(this,
                                  preferences,
                                  html_data,
                                  GURL(kBlockedPluginDataURL));

  registrar_.Add(this,
                 NotificationType::SHOULD_LOAD_PLUGINS,
                 NotificationService::AllSources());
}

BlockedPlugin::~BlockedPlugin() {}

void BlockedPlugin::BindWebFrame(WebFrame* frame) {
  BindToJavascript(frame, L"plugin");
  BindMethod("load", &BlockedPlugin::Load);
}

void BlockedPlugin::WillDestroyPlugin() {
  delete this;
}

void BlockedPlugin::Observe(NotificationType type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  if (type == NotificationType::SHOULD_LOAD_PLUGINS) {
    LoadPlugin();
  } else {
    NOTREACHED();
  }
}

void BlockedPlugin::Load(const CppArgumentList& args, CppVariant* result) {
  LoadPlugin();
}

void BlockedPlugin::LoadPlugin() {
  CHECK(plugin_);
  WebPluginContainer* container = plugin_->container();
  WebPlugin* new_plugin =
      render_view_->CreatePluginNoCheck(frame_,
                                        plugin_params_);
  if (new_plugin && new_plugin->initialize(container)) {
    container->setPlugin(new_plugin);
    container->invalidate();
    container->reportGeometry();
    plugin_->ReplayReceivedData(new_plugin);
    plugin_->destroy();
  }
}
