// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/missing_plugin.h"

#include "base/bind.h"
#include "base/json/string_escape.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/custom_menu_commands.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMenuItemInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/npapi/plugin_group.h"

using WebKit::WebContextMenuData;
using WebKit::WebFrame;
using WebKit::WebMenuItemInfo;
using WebKit::WebPlugin;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebScriptSource;
using WebKit::WebString;
using WebKit::WebVector;
using content::RenderThread;
using content::RenderView;
using webkit::WebViewPlugin;

namespace {
const MissingPlugin* g_last_active_menu = NULL;
}

// static
WebViewPlugin* MissingPlugin::Create(RenderView* render_view,
                                     WebFrame* frame,
                                     const WebPluginParams& params) {
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_BLOCKED_PLUGIN_HTML));

  DictionaryValue values;
  values.SetString("message", l10n_util::GetStringUTF8(IDS_PLUGIN_SEARCHING));

  // "t" is the id of the templates root node.
  std::string html_data =
      jstemplate_builder::GetI18nTemplateHtml(template_html, &values);

  // |missing_plugin| will destroy itself when its WebViewPlugin is going away.
  MissingPlugin* missing_plugin = new MissingPlugin(
      render_view, frame, params, html_data);
  return missing_plugin->plugin();
}

MissingPlugin::MissingPlugin(RenderView* render_view,
                             WebFrame* frame,
                             const WebPluginParams& params,
                             const std::string& html_data)
    : PluginPlaceholder(render_view, frame, params, html_data),
      mime_type_(params.mimeType),
      placeholder_routing_id_(RenderThread::Get()->GenerateRoutingID()) {
  RenderThread::Get()->AddRoute(placeholder_routing_id_, this);
  RenderThread::Get()->Send(new ChromeViewHostMsg_FindMissingPlugin(
      routing_id(), placeholder_routing_id_, mime_type_.utf8()));
}

MissingPlugin::~MissingPlugin() {
  RenderThread::Get()->RemoveRoute(placeholder_routing_id_);
}

void MissingPlugin::BindWebFrame(WebFrame* frame) {
  PluginPlaceholder::BindWebFrame(frame);
  BindCallback("hide", base::Bind(&MissingPlugin::HideCallback,
                                  base::Unretained(this)));
}

void MissingPlugin::HideCallback(const CppArgumentList& args,
                                 CppVariant* result) {
  RenderThread::Get()->RecordUserMetrics("MissingPlugin_Hide_Click");
  HidePluginInternal();
}

void MissingPlugin::ShowContextMenu(const WebKit::WebMouseEvent& event) {
  WebContextMenuData menu_data;

  WebVector<WebMenuItemInfo> custom_items(static_cast<size_t>(3));

  size_t i = 0;
  WebMenuItemInfo mime_type_item;
  mime_type_item.label = mime_type_;
  mime_type_item.hasTextDirectionOverride = false;
  mime_type_item.textDirection = WebKit::WebTextDirectionDefault;
  custom_items[i++] = mime_type_item;

  WebMenuItemInfo separator_item;
  separator_item.type = WebMenuItemInfo::Separator;
  custom_items[i++] = separator_item;

  WebMenuItemInfo hide_item;
  hide_item.action = chrome::MENU_COMMAND_PLUGIN_HIDE;
  hide_item.enabled = true;
  hide_item.label = WebString::fromUTF8(
      l10n_util::GetStringUTF8(IDS_CONTENT_CONTEXT_PLUGIN_HIDE).c_str());
  hide_item.hasTextDirectionOverride = false;
  hide_item.textDirection = WebKit::WebTextDirectionDefault;
  custom_items[i++] = hide_item;

  menu_data.customItems.swap(custom_items);
  menu_data.mousePosition = WebPoint(event.windowX, event.windowY);
  render_view()->ShowContextMenu(NULL, menu_data);
  g_last_active_menu = this;
}

bool MissingPlugin::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MissingPlugin, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_FoundMissingPlugin,
                        OnFoundMissingPlugin)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_DidNotFindMissingPlugin,
                        OnDidNotFindMissingPlugin)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void MissingPlugin::OnFoundMissingPlugin(const string16& plugin_name) {
  SetMessage(l10n_util::GetStringFUTF16(IDS_PLUGIN_FOUND, plugin_name));
}

void MissingPlugin::OnDidNotFindMissingPlugin() {
  SetMessage(l10n_util::GetStringUTF16(IDS_PLUGIN_NOT_FOUND));
}

void MissingPlugin::SetMessage(const string16& message) {
  message_ = message;
  if (!plugin()->web_view()->mainFrame()->isLoading())
    UpdateMessage();
}

void MissingPlugin::UpdateMessage() {
  DCHECK(!plugin()->web_view()->mainFrame()->isLoading());
  std::string script = "window.setMessage(" +
                       base::GetDoubleQuotedJson(message_) + ")";
  plugin()->web_view()->mainFrame()->executeScript(
      WebScriptSource(ASCIIToUTF16(script)));
}

void MissingPlugin::ContextMenuAction(unsigned id) {
  if (g_last_active_menu != this)
    return;
  if (id == chrome::MENU_COMMAND_PLUGIN_HIDE) {
    RenderThread::Get()->RecordUserMetrics("MissingPlugin_Hide_Menu");
    HidePluginInternal();
  } else {
    NOTREACHED();
  }
}

void MissingPlugin::DidFinishLoading() {
  if (message_.length() > 0)
    UpdateMessage();
}
