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
#include "chrome/common/plugin_group.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/plugins/webview_plugin.h"

using WebKit::WebCursorInfo;
using WebKit::WebFrame;
using WebKit::WebPlugin;
using WebKit::WebPluginContainer;
using WebKit::WebPluginParams;
using WebKit::WebURL;
using WebKit::WebView;

static const char* const kBlockedPluginDataURL = "chrome://blockedplugindata/";

BlockedPlugin::BlockedPlugin(RenderView* render_view,
                             WebFrame* frame,
                             const WebPluginParams& params,
                             PluginGroup* group)
    : render_view_(render_view),
      frame_(frame),
      plugin_params_(params) {
  plugin_ = new WebViewPlugin(this);

  WebView* web_view = plugin_->web_view();
  web_view->mainFrame()->setCanHaveScrollbars(false);

  int resource_id = IDR_BLOCKED_PLUGIN_HTML;
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id));

  DCHECK(!template_html.empty()) << "unable to load template. ID: "
                                 << resource_id;

  DictionaryValue values;
  values.SetString("loadPlugin",
      l10n_util::GetStringUTF16(IDS_PLUGIN_LOAD));
  values.SetString("updatePlugin",
      l10n_util::GetStringUTF16(IDS_PLUGIN_UPDATE));
  values.SetString("message",
      l10n_util::GetStringUTF16(IDS_BLOCKED_PLUGINS_MESSAGE));
  if (group)
    values.Set("pluginGroup", group->GetDataForUI());

  // "t" is the id of the templates root node.
  std::string htmlData = jstemplate_builder::GetTemplatesHtml(
      template_html, &values, "t");

  web_view->mainFrame()->loadHTMLString(htmlData,
                                        GURL(kBlockedPluginDataURL));

  registrar_.Add(this,
                 NotificationType::SHOULD_LOAD_PLUGINS,
                 NotificationService::AllSources());
}

void BlockedPlugin::BindWebFrame(WebFrame* frame) {
  BindToJavascript(frame, L"plugin");
  BindMethod("load", &BlockedPlugin::Load);
  BindMethod("update", &BlockedPlugin::Update);
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

void BlockedPlugin::Update(const CppArgumentList& args, CppVariant* result) {
  if (args.size() > 0) {
    CppVariant arg(args[0]);
    if (arg.isString()) {
      GURL url(arg.ToString());
      OpenURL(url);
    }
  }
}

void BlockedPlugin::OpenURL(GURL& url) {
  render_view_->Send(new ViewHostMsg_OpenURL(render_view_->routing_id(),
                                             url,
                                             GURL(),
                                             CURRENT_TAB));
}

void BlockedPlugin::LoadPlugin() {
  CHECK(plugin_);
  WebPluginContainer* container = plugin_->container();
  WebPlugin* new_plugin =
      render_view_->CreatePluginNoCheck(frame_,
                                        plugin_params_);
  if (new_plugin && new_plugin->initialize(container)) {
    container->setPlugin(new_plugin);
    plugin_->ReplayReceivedData(new_plugin);
    container->invalidate();
    container->reportGeometry();
    plugin_->destroy();
    render_view_->Send(
        new ViewHostMsg_BlockedPluginLoaded(render_view_->routing_id()));
  }
}
