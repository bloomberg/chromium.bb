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
#include "third_party/WebKit/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebMenuItemInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/plugins/plugin_group.h"
#include "webkit/glue/plugins/webview_plugin.h"
#include "webkit/glue/webpreferences.h"

using WebKit::WebContextMenuData;
using WebKit::WebFrame;
using WebKit::WebMenuItemInfo;
using WebKit::WebPlugin;
using WebKit::WebPluginContainer;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebString;
using WebKit::WebVector;

static const char* const kBlockedPluginDataURL = "chrome://blockedplugindata/";
static const unsigned kMenuActionLoad = 1;
static const unsigned kMenuActionRemove = 2;

BlockedPlugin::BlockedPlugin(RenderView* render_view,
                             WebFrame* frame,
                             const PluginGroup& info,
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
  name_ = info.GetGroupName();
  values.SetString("name", name_);

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

BlockedPlugin::~BlockedPlugin() {
  render_view_->CustomMenuListenerDestroyed(this);
}

void BlockedPlugin::BindWebFrame(WebFrame* frame) {
  BindToJavascript(frame, "plugin");
  BindMethod("load", &BlockedPlugin::Load);
}

void BlockedPlugin::WillDestroyPlugin() {
  delete this;
}

void BlockedPlugin::ShowContextMenu(const WebKit::WebMouseEvent& event) {
  WebContextMenuData menu_data;
  WebVector<WebMenuItemInfo> custom_items(static_cast<size_t>(4));

  WebMenuItemInfo name_item;
  name_item.label = name_;
  custom_items[0] = name_item;

  WebMenuItemInfo separator_item;
  separator_item.type = WebMenuItemInfo::Separator;
  custom_items[1] = separator_item;

  WebMenuItemInfo run_item;
  run_item.action = kMenuActionLoad;
  run_item.enabled = true;
  run_item.label = WebString::fromUTF8(
      l10n_util::GetStringUTF8(IDS_CONTENT_CONTEXT_PLUGIN_RUN).c_str());
  custom_items[2] = run_item;

  WebMenuItemInfo hide_item;
  hide_item.action = kMenuActionRemove;
  hide_item.enabled = true;
  hide_item.label = WebString::fromUTF8(
      l10n_util::GetStringUTF8(IDS_CONTENT_CONTEXT_PLUGIN_HIDE).c_str());
  custom_items[3] = hide_item;

  menu_data.customItems.swap(custom_items);
  menu_data.mousePosition = WebPoint(event.windowX, event.windowY);
  render_view_->showContextMenu(NULL, menu_data);
  render_view_->CustomMenuListenerInstall(this);
}

void BlockedPlugin::MenuItemSelected(unsigned id) {
  if (id == kMenuActionLoad) {
    LoadPlugin();
  } else if (id == kMenuActionRemove) {
    HidePlugin();
  } else {
    NOTREACHED();
  }
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

void BlockedPlugin::HidePlugin() {
  CHECK(plugin_);
  WebPluginContainer* container = plugin_->container();
  container->element().setAttribute("style", "display: none;");
}

