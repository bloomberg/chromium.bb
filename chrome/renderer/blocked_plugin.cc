// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/blocked_plugin.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMenuItemInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRegularExpression.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCaseSensitivity.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/plugins/npapi/plugin_group.h"
#include "webkit/plugins/npapi/webview_plugin.h"

using WebKit::WebContextMenuData;
using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebMenuItemInfo;
using WebKit::WebNode;
using WebKit::WebPlugin;
using WebKit::WebPluginContainer;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebRegularExpression;
using WebKit::WebString;
using WebKit::WebVector;

static const char* const kBlockedPluginDataURL = "chrome://blockedplugindata/";
static const unsigned kMenuActionLoad = 1;
static const unsigned kMenuActionRemove = 2;

BlockedPlugin::BlockedPlugin(RenderView* render_view,
                             WebFrame* frame,
                             const webkit::npapi::PluginGroup& info,
                             const WebPluginParams& params,
                             const WebPreferences& preferences,
                             int template_id,
                             const string16& message)
    : RenderViewObserver(render_view),
      frame_(frame),
      plugin_params_(params),
      custom_menu_showing_(false) {
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

  plugin_ = webkit::npapi::WebViewPlugin::Create(this,
                                                 preferences,
                                                 html_data,
                                                 GURL(kBlockedPluginDataURL));
}

BlockedPlugin::~BlockedPlugin() {
}

void BlockedPlugin::BindWebFrame(WebFrame* frame) {
  BindToJavascript(frame, "plugin");
  BindMethod("load", &BlockedPlugin::Load);
  BindMethod("hide", &BlockedPlugin::Hide);
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
  render_view()->showContextMenu(NULL, menu_data);
  custom_menu_showing_ = true;
}

bool BlockedPlugin::OnMessageReceived(const IPC::Message& message) {
  if (custom_menu_showing_ &&
      message.type() == ViewMsg_CustomContextMenuAction::ID) {
    ViewMsg_CustomContextMenuAction::Dispatch(
        &message, this, this, &BlockedPlugin::OnMenuItemSelected);
    return true;
  }

  // Don't want to swallow these messages.
  if (message.type() == ViewMsg_LoadBlockedPlugins::ID) {
    LoadPlugin();
  } else if (message.type() == ViewMsg_ContextMenuClosed::ID) {
    custom_menu_showing_ = false;
  }

  return false;
}

void BlockedPlugin::OnMenuItemSelected(unsigned id) {
  if (id == kMenuActionLoad) {
    LoadPlugin();
  } else if (id == kMenuActionRemove) {
    HidePlugin();
  } else {
    NOTREACHED();
  }
}

void BlockedPlugin::LoadPlugin() {
  CHECK(plugin_);
  WebPluginContainer* container = plugin_->container();
  WebPlugin* new_plugin =
      render_view()->CreatePluginNoCheck(frame_, plugin_params_);
  if (new_plugin && new_plugin->initialize(container)) {
    container->setPlugin(new_plugin);
    container->invalidate();
    container->reportGeometry();
    plugin_->ReplayReceivedData(new_plugin);
    plugin_->destroy();
  }
}

void BlockedPlugin::Load(const CppArgumentList& args, CppVariant* result) {
  LoadPlugin();
}

void BlockedPlugin::Hide(const CppArgumentList& args, CppVariant* result) {
  HidePlugin();
}

void BlockedPlugin::HidePlugin() {
  CHECK(plugin_);
  WebPluginContainer* container = plugin_->container();
  WebElement element = container->element();
  element.setAttribute("style", "display: none;");
  // If we have a width and height, search for a parent (often <div>) with the
  // same dimensions. If we find such a parent, hide that as well.
  // This makes much more uncovered page content usable (including clickable)
  // as opposed to merely visible.
  // TODO(cevans) -- it's a foul heurisitc but we're going to tolerate it for
  // now for these reasons:
  // 1) Makes the user experience better.
  // 2) Foulness is encapsulated within this single function.
  // 3) Confidence in no fasle positives.
  // 4) Seems to have a good / low false negative rate at this time.
  if (element.hasAttribute("width") && element.hasAttribute("height")) {
    std::string width_str("width:[\\s]*");
    width_str += element.getAttribute("width").utf8().data();
    width_str += "[\\s]*px";
    WebRegularExpression width_regex(WebString::fromUTF8(width_str.c_str()),
                                     WebKit::WebTextCaseSensitive);
    std::string height_str("height:[\\s]*");
    height_str += element.getAttribute("height").utf8().data();
    height_str += "[\\s]*px";
    WebRegularExpression height_regex(WebString::fromUTF8(height_str.c_str()),
                                      WebKit::WebTextCaseSensitive);
    WebNode parent = element;
    while (!parent.parentNode().isNull()) {
      parent = parent.parentNode();
      if (!parent.isElementNode())
        continue;
      element = parent.toConst<WebElement>();
      if (element.hasAttribute("style")) {
        WebString style_str = element.getAttribute("style");
        if (width_regex.match(style_str) >= 0 &&
            height_regex.match(style_str) >= 0)
          element.setAttribute("style", "display: none;");
      }
    }
  }
}

