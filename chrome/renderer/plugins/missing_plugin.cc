// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/missing_plugin.h"

#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/renderer/custom_menu_commands.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMenuItemInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/plugins/npapi/plugin_group.h"

using WebKit::WebContextMenuData;
using WebKit::WebFrame;
using WebKit::WebMenuItemInfo;
using WebKit::WebPlugin;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebString;
using WebKit::WebVector;
using content::RenderThread;
using webkit::WebViewPlugin;

namespace {
const MissingPlugin* g_last_active_menu = NULL;
}

// static
WebViewPlugin* MissingPlugin::Create(content::RenderView* render_view,
                                     WebFrame* frame,
                                     const WebPluginParams& params) {
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_BLOCKED_PLUGIN_HTML));

  DictionaryValue values;
  values.SetString("message", l10n_util::GetStringUTF8(IDS_PLUGIN_NOT_FOUND));

  // "t" is the id of the templates root node.
  std::string html_data =
      jstemplate_builder::GetI18nTemplateHtml(template_html, &values);

  // |missing_plugin| will destroy itself when its WebViewPlugin is going away.
  MissingPlugin* missing_plugin = new MissingPlugin(render_view, frame, params,
                                                    html_data);
  return missing_plugin->plugin();
}

MissingPlugin::MissingPlugin(content::RenderView* render_view,
                             WebFrame* frame,
                             const WebPluginParams& params,
                             const std::string& html_data)
    : PluginPlaceholder(render_view, frame, params, html_data),
      mime_type_(params.mimeType) {
}

MissingPlugin::~MissingPlugin() {
}

void MissingPlugin::BindWebFrame(WebFrame* frame) {
  PluginPlaceholder::BindWebFrame(frame);
  BindMethod("hide", &MissingPlugin::HideCallback);
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

