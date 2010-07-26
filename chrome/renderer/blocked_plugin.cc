// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/blocked_plugin.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/file_path.h"
#include "base/string_piece.h"
#include "chrome/common/jstemplate_builder.h"
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
                            const WebPluginParams& params)
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

  DictionaryValue localized_strings;
  localized_strings.SetString(L"loadPlugin",
      l10n_util::GetString(IDS_PLUGIN_LOAD));

  // "t" is the id of the templates root node.
  std::string htmlData = jstemplate_builder::GetTemplatesHtml(
      template_html, &localized_strings, "t");

  web_view->mainFrame()->loadHTMLString(htmlData,
                                        GURL(kBlockedPluginDataURL));
}

void BlockedPlugin::BindWebFrame(WebFrame* frame) {
  BindToJavascript(frame, L"plugin");
  BindMethod("load", &BlockedPlugin::Load);
}

void BlockedPlugin::WillDestroyPlugin() {
  delete this;
}

void BlockedPlugin::Load(const CppArgumentList& args, CppVariant* result) {
  LoadPlugin();
}

void BlockedPlugin::LoadPlugin() {
  CHECK(plugin_);
  WebPluginContainer* container = plugin_->container();
  WebPlugin* new_plugin =
      render_view_->CreatePluginInternal(frame_,
                                         plugin_params_,
                                         std::string(),
                                         FilePath());
  if (new_plugin && new_plugin->initialize(container)) {
    container->setPlugin(new_plugin);
    container->invalidate();
    container->reportGeometry();
    plugin_->destroy();
  }
}
