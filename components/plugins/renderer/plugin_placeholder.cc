// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plugins/renderer/plugin_placeholder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/json/string_escape.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "gin/object_template_builder.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/re2/re2/re2.h"

using base::UserMetricsAction;
using blink::WebElement;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebNode;
using blink::WebPlugin;
using blink::WebPluginContainer;
using blink::WebPluginParams;
using blink::WebScriptSource;
using blink::WebURLRequest;
using content::RenderThread;

namespace plugins {

gin::WrapperInfo PluginPlaceholder::kWrapperInfo = {gin::kEmbedderNativeGin};

PluginPlaceholder::PluginPlaceholder(content::RenderFrame* render_frame,
                                     WebLocalFrame* frame,
                                     const WebPluginParams& params,
                                     const std::string& html_data,
                                     GURL placeholderDataUrl)
    : content::RenderFrameObserver(render_frame),
      frame_(frame),
      plugin_params_(params),
      plugin_(WebViewPlugin::Create(this,
                                    render_frame->GetWebkitPreferences(),
                                    html_data,
                                    placeholderDataUrl)),
      is_blocked_for_prerendering_(false),
      allow_loading_(false),
      hidden_(false),
      finished_loading_(false) {}

PluginPlaceholder::~PluginPlaceholder() {}

gin::ObjectTemplateBuilder PluginPlaceholder::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<PluginPlaceholder>::GetObjectTemplateBuilder(isolate)
      .SetMethod("load", &PluginPlaceholder::LoadCallback)
      .SetMethod("hide", &PluginPlaceholder::HideCallback)
      .SetMethod("didFinishLoading",
                 &PluginPlaceholder::DidFinishLoadingCallback);
}

void PluginPlaceholder::ReplacePlugin(WebPlugin* new_plugin) {
  CHECK(plugin_);
  if (!new_plugin) return;
  WebPluginContainer* container = plugin_->container();
  // Set the new plug-in on the container before initializing it.
  container->setPlugin(new_plugin);
  // Save the element in case the plug-in is removed from the page during
  // initialization.
  WebElement element = container->element();
  if (!new_plugin->initialize(container)) {
    // We couldn't initialize the new plug-in. Restore the old one and abort.
    container->setPlugin(plugin_);
    return;
  }

  // The plug-in has been removed from the page. Destroy the old plug-in. We
  // will be destroyed as soon as V8 garbage collects us.
  if (!element.pluginContainer()) {
    plugin_->destroy();
    return;
  }

  // During initialization, the new plug-in might have replaced itself in turn
  // with another plug-in. Make sure not to use the passed in |new_plugin| after
  // this point.
  new_plugin = container->plugin();

  plugin_->RestoreTitleText();
  container->invalidate();
  container->reportGeometry();
  plugin_->ReplayReceivedData(new_plugin);
  plugin_->destroy();
}

void PluginPlaceholder::HidePlugin() {
  hidden_ = true;
  if (!plugin_)
    return;
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
    base::TrimWhitespace(width_str, base::TRIM_TRAILING, &width_str);
    width_str += "[\\s]*px";
    std::string height_str("height:[\\s]*");
    height_str += element.getAttribute("height").utf8().data();
    if (EndsWith(height_str, "px", false)) {
      height_str = height_str.substr(0, height_str.length() - 2);
    }
    base::TrimWhitespace(height_str, base::TRIM_TRAILING, &height_str);
    height_str += "[\\s]*px";
    WebNode parent = element;
    while (!parent.parentNode().isNull()) {
      parent = parent.parentNode();
      if (!parent.isElementNode())
        continue;
      element = parent.toConst<WebElement>();
      if (element.hasAttribute("style")) {
        std::string style_str = element.getAttribute("style").utf8();
        if (RE2::PartialMatch(style_str, width_str) &&
            RE2::PartialMatch(style_str, height_str))
          element.setAttribute("style", "display: none;");
      }
    }
  }
}

void PluginPlaceholder::SetMessage(const base::string16& message) {
  message_ = message;
  if (finished_loading_)
    UpdateMessage();
}

void PluginPlaceholder::UpdateMessage() {
  if (!plugin_)
    return;
  std::string script =
      "window.setMessage(" + base::GetQuotedJSONString(message_) + ")";
  plugin_->web_view()->mainFrame()->executeScript(
      WebScriptSource(base::UTF8ToUTF16(script)));
}

void PluginPlaceholder::ShowContextMenu(const WebMouseEvent& event) {
  // Does nothing by default. Will be overridden if a specific browser wants
  // a context menu.
  return;
}

void PluginPlaceholder::PluginDestroyed() {
  plugin_ = NULL;
}

void PluginPlaceholder::OnDestruct() {
  frame_ = NULL;
}

void PluginPlaceholder::OnLoadBlockedPlugins(const std::string& identifier) {
  if (!identifier.empty() && identifier != identifier_)
    return;

  RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Load_UI"));
  LoadPlugin();
}

void PluginPlaceholder::OnSetIsPrerendering(bool is_prerendering) {
  // Prerendering can only be enabled prior to a RenderView's first navigation,
  // so no BlockedPlugin should see the notification that enables prerendering.
  DCHECK(!is_prerendering);
  if (is_blocked_for_prerendering_ && !is_prerendering)
    LoadPlugin();
}

void PluginPlaceholder::LoadPlugin() {
  // This is not strictly necessary but is an important defense in case the
  // event propagation changes between "close" vs. "click-to-play".
  if (hidden_)
    return;
  if (!plugin_)
    return;
  if (!allow_loading_) {
    NOTREACHED();
    return;
  }

  // TODO(mmenke):  In the case of prerendering, feed into
  //                ChromeContentRendererClient::CreatePlugin instead, to
  //                reduce the chance of future regressions.
  WebPlugin* plugin =
      render_frame()->CreatePlugin(frame_, plugin_info_, plugin_params_);
  ReplacePlugin(plugin);
}

void PluginPlaceholder::LoadCallback() {
  RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Load_Click"));
  LoadPlugin();
}

void PluginPlaceholder::HideCallback() {
  RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Hide_Click"));
  HidePlugin();
}

void PluginPlaceholder::DidFinishLoadingCallback() {
  finished_loading_ = true;
  if (message_.length() > 0)
    UpdateMessage();
}

void PluginPlaceholder::SetPluginInfo(
    const content::WebPluginInfo& plugin_info) {
  plugin_info_ = plugin_info;
}

const content::WebPluginInfo& PluginPlaceholder::GetPluginInfo() const {
  return plugin_info_;
}

void PluginPlaceholder::SetIdentifier(const std::string& identifier) {
  identifier_ = identifier;
}

blink::WebLocalFrame* PluginPlaceholder::GetFrame() { return frame_; }

const blink::WebPluginParams& PluginPlaceholder::GetPluginParams() const {
  return plugin_params_;
}

}  // namespace plugins
