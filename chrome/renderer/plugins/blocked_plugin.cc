// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/blocked_plugin.h"

#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/custom_menu_commands.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMenuItemInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/webview_plugin.h"

using WebKit::WebContextMenuData;
using WebKit::WebFrame;
using WebKit::WebMenuItemInfo;
using WebKit::WebPlugin;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebString;
using WebKit::WebURLRequest;
using WebKit::WebVector;
using content::RenderThread;
using webkit::WebViewPlugin;

namespace {
const BlockedPlugin* g_last_active_menu = NULL;
}

// static
WebViewPlugin* BlockedPlugin::Create(content::RenderView* render_view,
                                     WebFrame* frame,
                                     const WebPluginParams& params,
                                     const webkit::WebPluginInfo& plugin,
                                     const webkit::npapi::PluginGroup* group,
                                     int template_id,
                                     int message_id,
                                     bool is_blocked_for_prerendering,
                                     bool allow_loading) {
  string16 name = group->GetGroupName();
  string16 message = l10n_util::GetStringFUTF16(message_id, name);

  DictionaryValue values;
  values.SetString("message", message);
  values.SetString("name", name);
  values.SetString("hide", l10n_util::GetStringUTF8(IDS_PLUGIN_HIDE));

  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(template_id));

  DCHECK(!template_html.empty()) << "unable to load template. ID: "
                                 << template_id;
  // "t" is the id of the templates root node.
  std::string html_data = jstemplate_builder::GetI18nTemplateHtml(
      template_html, &values);

  // |blocked_plugin| will destroy itself when its WebViewPlugin is going away.
  BlockedPlugin* blocked_plugin = new BlockedPlugin(
      render_view, frame, params, html_data, plugin, name,
      is_blocked_for_prerendering, allow_loading);
  return blocked_plugin->plugin();
}

BlockedPlugin::BlockedPlugin(content::RenderView* render_view,
                             WebFrame* frame,
                             const WebPluginParams& params,
                             const std::string& html_data,
                             const webkit::WebPluginInfo& info,
                             const string16& name,
                             bool is_blocked_for_prerendering,
                             bool allow_loading)
    : PluginPlaceholder(render_view, frame, params, html_data),
      plugin_info_(info),
      name_(name),
      is_blocked_for_prerendering_(is_blocked_for_prerendering),
      hidden_(false),
      allow_loading_(allow_loading) {
}

BlockedPlugin::~BlockedPlugin() {
}

void BlockedPlugin::BindWebFrame(WebFrame* frame) {
  PluginPlaceholder::BindWebFrame(frame);
  BindMethod("load", &BlockedPlugin::LoadCallback);
  BindMethod("hide", &BlockedPlugin::HideCallback);
  BindMethod("openURL", &BlockedPlugin::OpenUrlCallback);
}

void BlockedPlugin::ShowContextMenu(const WebKit::WebMouseEvent& event) {
  WebContextMenuData menu_data;

  size_t num_items = name_.empty() ? 2u : 4u;
  WebVector<WebMenuItemInfo> custom_items(num_items);

  size_t i = 0;
  if (!name_.empty()) {
    WebMenuItemInfo name_item;
    name_item.label = name_;
    name_item.hasTextDirectionOverride = false;
    name_item.textDirection =  WebKit::WebTextDirectionDefault;
    custom_items[i++] = name_item;

    WebMenuItemInfo separator_item;
    separator_item.type = WebMenuItemInfo::Separator;
    custom_items[i++] = separator_item;
  }

  WebMenuItemInfo run_item;
  run_item.action = chrome::MENU_COMMAND_PLUGIN_RUN;
  // Disable this menu item if the plugin is blocked by policy.
  run_item.enabled = allow_loading_;
  run_item.label = WebString::fromUTF8(
      l10n_util::GetStringUTF8(IDS_CONTENT_CONTEXT_PLUGIN_RUN).c_str());
  run_item.hasTextDirectionOverride = false;
  run_item.textDirection =  WebKit::WebTextDirectionDefault;
  custom_items[i++] = run_item;

  WebMenuItemInfo hide_item;
  hide_item.action = chrome::MENU_COMMAND_PLUGIN_HIDE;
  hide_item.enabled = true;
  hide_item.label = WebString::fromUTF8(
      l10n_util::GetStringUTF8(IDS_CONTENT_CONTEXT_PLUGIN_HIDE).c_str());
  hide_item.hasTextDirectionOverride = false;
  hide_item.textDirection =  WebKit::WebTextDirectionDefault;
  custom_items[i++] = hide_item;

  menu_data.customItems.swap(custom_items);
  menu_data.mousePosition = WebPoint(event.windowX, event.windowY);
  render_view()->ShowContextMenu(NULL, menu_data);
  g_last_active_menu = this;
}

bool BlockedPlugin::OnMessageReceived(const IPC::Message& message) {
  // We don't swallow ViewMsg_CustomContextMenuAction because we listen for all
  // custom menu IDs, and not just our own. We don't swallow
  // ViewMsg_LoadBlockedPlugins because multiple blocked plugins have an
  // interest in it.
  IPC_BEGIN_MESSAGE_MAP(BlockedPlugin, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_LoadBlockedPlugins, OnLoadBlockedPlugins)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetIsPrerendering, OnSetIsPrerendering)
  IPC_END_MESSAGE_MAP()

  return false;
}

void BlockedPlugin::ContextMenuAction(unsigned id) {
  if (g_last_active_menu != this)
    return;
  switch (id) {
    case chrome::MENU_COMMAND_PLUGIN_RUN: {
      RenderThread::Get()->RecordUserMetrics("Plugin_Load_Menu");
      LoadPlugin();
      break;
    }
    case chrome::MENU_COMMAND_PLUGIN_HIDE: {
      RenderThread::Get()->RecordUserMetrics("Plugin_Hide_Menu");
      HidePlugin();
      break;
    }
    default:
      NOTREACHED();
  }
}

void BlockedPlugin::OnLoadBlockedPlugins() {
  RenderThread::Get()->RecordUserMetrics("Plugin_Load_UI");
  LoadPlugin();
}

void BlockedPlugin::OnSetIsPrerendering(bool is_prerendering) {
  // Prerendering can only be enabled prior to a RenderView's first navigation,
  // so no BlockedPlugin should see the notification that enables prerendering.
  DCHECK(!is_prerendering);
  if (is_blocked_for_prerendering_ && !is_prerendering)
    LoadPlugin();
}

void BlockedPlugin::LoadPlugin() {
  // This is not strictly necessary but is an important defense in case the
  // event propagation changes between "close" vs. "click-to-play".
  if (hidden_)
    return;
  if (!allow_loading_)
    return;
  LoadPluginInternal(plugin_info_);
}

void BlockedPlugin::LoadCallback(const CppArgumentList& args,
                                 CppVariant* result) {
  RenderThread::Get()->RecordUserMetrics("Plugin_Load_Click");
  LoadPlugin();
}

void BlockedPlugin::HideCallback(const CppArgumentList& args,
                                 CppVariant* result) {
  RenderThread::Get()->RecordUserMetrics("Plugin_Hide_Click");
  HidePlugin();
}

void BlockedPlugin::OpenUrlCallback(const CppArgumentList& args,
                                    CppVariant* result) {
  if (args.size() < 1) {
    NOTREACHED();
    return;
  }
  if (!args[0].isString()) {
    NOTREACHED();
    return;
  }

  GURL url(args[0].ToString());
  WebURLRequest request;
  request.initialize();
  request.setURL(url);
  render_view()->LoadURLExternally(
      frame(), request, WebKit::WebNavigationPolicyNewForegroundTab);
}

void BlockedPlugin::HidePlugin() {
  hidden_ = true;
  HidePluginInternal();
}
