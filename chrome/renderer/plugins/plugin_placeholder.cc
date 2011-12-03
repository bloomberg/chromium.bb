// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/plugins/plugin_placeholder.h"

#include "base/string_util.h"
#include "chrome/renderer/plugins/plugin_uma.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRegularExpression.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextCaseSensitivity.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebElement;
using WebKit::WebFrame;
using WebKit::WebMouseEvent;
using WebKit::WebNode;
using WebKit::WebPlugin;
using WebKit::WebPluginContainer;
using WebKit::WebPluginParams;
using WebKit::WebRegularExpression;
using WebKit::WebString;
using webkit::WebPluginInfo;
using webkit::WebViewPlugin;

static const char* const kPluginPlaceholderDataURL =
    "chrome://pluginplaceholderdata/";

PluginPlaceholder::PluginPlaceholder(content::RenderView* render_view,
                                     WebFrame* frame,
                                     const WebPluginParams& params,
                                     const std::string& html_data)
    : content::RenderViewObserver(render_view),
      frame_(frame),
      plugin_params_(params),
      plugin_(WebViewPlugin::Create(
          this, render_view->GetWebkitPreferences(), html_data,
          GURL(kPluginPlaceholderDataURL))) {
}

PluginPlaceholder::~PluginPlaceholder() {
}

void PluginPlaceholder::BindWebFrame(WebFrame* frame) {
  BindToJavascript(frame, "plugin");
}

void PluginPlaceholder::LoadPluginInternal(const WebPluginInfo& plugin_info) {
  CHECK(plugin_);
  WebPluginContainer* container = plugin_->container();
  WebPlugin* new_plugin =
      render_view()->CreatePlugin(frame_, plugin_info, plugin_params_);
  if (new_plugin && new_plugin->initialize(container)) {
    plugin_->RestoreTitleText();
    container->setPlugin(new_plugin);
    container->invalidate();
    container->reportGeometry();
    plugin_->ReplayReceivedData(new_plugin);
    plugin_->destroy();
  } else {
    MissingPluginReporter::GetInstance()->ReportPluginMissing(
        plugin_params_.mimeType.utf8(),
        plugin_params_.url);
  }
}

void PluginPlaceholder::HidePluginInternal() {
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
    if (EndsWith(width_str, "px", false)) {
      width_str = width_str.substr(0, width_str.length() - 2);
    }
    TrimWhitespace(width_str, TRIM_TRAILING, &width_str);
    width_str += "[\\s]*px";
    WebRegularExpression width_regex(WebString::fromUTF8(width_str.c_str()),
                                     WebKit::WebTextCaseSensitive);
    std::string height_str("height:[\\s]*");
    height_str += element.getAttribute("height").utf8().data();
    if (EndsWith(height_str, "px", false)) {
      height_str = height_str.substr(0, height_str.length() - 2);
    }
    TrimWhitespace(height_str, TRIM_TRAILING, &height_str);
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

void PluginPlaceholder::WillDestroyPlugin() {
  delete this;
}

void PluginPlaceholder::ShowContextMenu(const WebMouseEvent& event) {
}

void PluginPlaceholder::DidFinishLoading() {
}
