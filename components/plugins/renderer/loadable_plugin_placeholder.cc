// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plugins/renderer/loadable_plugin_placeholder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/json/string_escape.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/WebKit/public/web/WebDOMMessageEvent.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSerializedScriptValue.h"
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
using content::PluginInstanceThrottler;
using content::RenderThread;

namespace plugins {

#if defined(ENABLE_PLUGINS)
void LoadablePluginPlaceholder::BlockForPowerSaverPoster() {
  DCHECK(!is_blocked_for_power_saver_poster_);
  is_blocked_for_power_saver_poster_ = true;

  render_frame()->RegisterPeripheralPlugin(
      GURL(GetPluginParams().url).GetOrigin(),
      base::Bind(&LoadablePluginPlaceholder::MarkPluginEssential,
                 weak_factory_.GetWeakPtr(),
                 PluginInstanceThrottler::UNTHROTTLE_METHOD_BY_WHITELIST));
}

void LoadablePluginPlaceholder::SetPremadePlugin(
    content::PluginInstanceThrottler* throttler) {
  DCHECK(throttler);
  DCHECK(!premade_throttler_);
  premade_throttler_ = throttler;
}
#endif

LoadablePluginPlaceholder::LoadablePluginPlaceholder(
    content::RenderFrame* render_frame,
    WebLocalFrame* frame,
    const WebPluginParams& params,
    const std::string& html_data,
    GURL placeholderDataUrl)
    : PluginPlaceholder(render_frame,
                        frame,
                        params,
                        html_data,
                        placeholderDataUrl),
      is_blocked_for_background_tab_(false),
      is_blocked_for_prerendering_(false),
      is_blocked_for_power_saver_poster_(false),
      power_saver_enabled_(false),
      premade_throttler_(nullptr),
      allow_loading_(false),
      hidden_(false),
      finished_loading_(false),
      weak_factory_(this) {
}

LoadablePluginPlaceholder::~LoadablePluginPlaceholder() {
}

#if defined(ENABLE_PLUGINS)
void LoadablePluginPlaceholder::MarkPluginEssential(
    PluginInstanceThrottler::PowerSaverUnthrottleMethod method) {
  if (!power_saver_enabled_)
    return;

  power_saver_enabled_ = false;

  if (premade_throttler_)
    premade_throttler_->MarkPluginEssential(method);
  else
    PluginInstanceThrottler::RecordUnthrottleMethodMetric(method);

  if (is_blocked_for_power_saver_poster_) {
    is_blocked_for_power_saver_poster_ = false;
    if (!LoadingBlocked())
      LoadPlugin();
  }
}
#endif

void LoadablePluginPlaceholder::BindWebFrame(blink::WebFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  DCHECK(!context.IsEmpty());

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "plugin"),
              gin::CreateHandle(isolate, this).ToV8());
}

gin::ObjectTemplateBuilder LoadablePluginPlaceholder::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<PluginPlaceholder>::GetObjectTemplateBuilder(isolate)
      .SetMethod("load", &LoadablePluginPlaceholder::LoadCallback)
      .SetMethod("hide", &LoadablePluginPlaceholder::HideCallback)
      .SetMethod("didFinishLoading",
                 &LoadablePluginPlaceholder::DidFinishLoadingCallback);
}

void LoadablePluginPlaceholder::ReplacePlugin(WebPlugin* new_plugin) {
  CHECK(plugin());
  if (!new_plugin)
    return;
  WebPluginContainer* container = plugin()->container();
  // Set the new plugin on the container before initializing it.
  container->setPlugin(new_plugin);
  // Save the element in case the plugin is removed from the page during
  // initialization.
  WebElement element = container->element();
  bool plugin_needs_initialization =
      !premade_throttler_ || new_plugin != premade_throttler_->GetWebPlugin();
  if (plugin_needs_initialization && !new_plugin->initialize(container)) {
    // We couldn't initialize the new plugin. Restore the old one and abort.
    container->setPlugin(plugin());
    return;
  }

  // The plugin has been removed from the page. Destroy the old plugin. We
  // will be destroyed as soon as V8 garbage collects us.
  if (!element.pluginContainer()) {
    plugin()->destroy();
    return;
  }

  // During initialization, the new plugin might have replaced itself in turn
  // with another plugin. Make sure not to use the passed in |new_plugin| after
  // this point.
  new_plugin = container->plugin();

  plugin()->RestoreTitleText();
  container->invalidate();
  container->reportGeometry();
  plugin()->ReplayReceivedData(new_plugin);
  plugin()->destroy();
}

void LoadablePluginPlaceholder::HidePlugin() {
  hidden_ = true;
  if (!plugin())
    return;
  WebPluginContainer* container = plugin()->container();
  WebElement element = container->element();
  element.setAttribute("style", "display: none;");
  // If we have a width and height, search for a parent (often <div>) with the
  // same dimensions. If we find such a parent, hide that as well.
  // This makes much more uncovered page content usable (including clickable)
  // as opposed to merely visible.
  // TODO(cevans) -- it's a foul heuristic but we're going to tolerate it for
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

void LoadablePluginPlaceholder::SetMessage(const base::string16& message) {
  message_ = message;
  if (finished_loading_)
    UpdateMessage();
}

void LoadablePluginPlaceholder::UpdateMessage() {
  if (!plugin())
    return;
  std::string script =
      "window.setMessage(" + base::GetQuotedJSONString(message_) + ")";
  plugin()->web_view()->mainFrame()->executeScript(
      WebScriptSource(base::UTF8ToUTF16(script)));
}

void LoadablePluginPlaceholder::PluginDestroyed() {
#if defined(ENABLE_PLUGINS)
  if (power_saver_enabled_) {
    if (premade_throttler_) {
      // Since the premade plugin has been detached from the container, it will
      // not be automatically destroyed along with the page.
      premade_throttler_->GetWebPlugin()->destroy();
      premade_throttler_ = nullptr;
    } else if (is_blocked_for_power_saver_poster_) {
      // Record the NEVER unthrottle count only if there is no throttler.
      PluginInstanceThrottler::RecordUnthrottleMethodMetric(
          PluginInstanceThrottler::UNTHROTTLE_METHOD_NEVER);
    }

    // Prevent processing subsequent calls to MarkPluginEssential.
    power_saver_enabled_ = false;
  }
#endif

  PluginPlaceholder::PluginDestroyed();
}

v8::Local<v8::Object> LoadablePluginPlaceholder::GetV8ScriptableObject(
    v8::Isolate* isolate) const {
#if defined(ENABLE_PLUGINS)
  // Pass through JavaScript access to the underlying throttled plugin.
  if (premade_throttler_ && premade_throttler_->GetWebPlugin()) {
    return premade_throttler_->GetWebPlugin()->v8ScriptableObject(isolate);
  }
#endif
  return v8::Local<v8::Object>();
}

void LoadablePluginPlaceholder::WasShown() {
  if (is_blocked_for_background_tab_) {
    is_blocked_for_background_tab_ = false;
    if (!LoadingBlocked())
      LoadPlugin();
  }
}

void LoadablePluginPlaceholder::OnLoadBlockedPlugins(
    const std::string& identifier) {
  if (!identifier.empty() && identifier != identifier_)
    return;

  RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Load_UI"));
  LoadPlugin();
}

void LoadablePluginPlaceholder::OnSetIsPrerendering(bool is_prerendering) {
  // Prerendering can only be enabled prior to a RenderView's first navigation,
  // so no BlockedPlugin should see the notification that enables prerendering.
  DCHECK(!is_prerendering);
  if (is_blocked_for_prerendering_) {
    is_blocked_for_prerendering_ = false;
    if (!LoadingBlocked())
      LoadPlugin();
  }
}

void LoadablePluginPlaceholder::LoadPlugin() {
  // This is not strictly necessary but is an important defense in case the
  // event propagation changes between "close" vs. "click-to-play".
  if (hidden_)
    return;
  if (!plugin())
    return;
  if (!allow_loading_) {
    NOTREACHED();
    return;
  }

  if (premade_throttler_) {
    premade_throttler_->SetHiddenForPlaceholder(false /* hidden */);
    ReplacePlugin(premade_throttler_->GetWebPlugin());
    premade_throttler_ = nullptr;
  } else {
    ReplacePlugin(CreatePlugin());
  }
}

void LoadablePluginPlaceholder::LoadCallback() {
  RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Load_Click"));
#if defined(ENABLE_PLUGINS)
  // If the user specifically clicks on the plugin content's placeholder,
  // disable power saver throttling for this instance.
  MarkPluginEssential(PluginInstanceThrottler::UNTHROTTLE_METHOD_BY_CLICK);
#endif
  LoadPlugin();
}

void LoadablePluginPlaceholder::HideCallback() {
  RenderThread::Get()->RecordAction(UserMetricsAction("Plugin_Hide_Click"));
  HidePlugin();
}

void LoadablePluginPlaceholder::DidFinishLoadingCallback() {
  finished_loading_ = true;
  if (message_.length() > 0)
    UpdateMessage();

  // Wait for the placeholder to finish loading to hide the premade plugin.
  // This is necessary to prevent a flicker.
  if (premade_throttler_ && power_saver_enabled_)
    premade_throttler_->SetHiddenForPlaceholder(true /* hidden */);

  // Set an attribute and post an event, so browser tests can wait for the
  // placeholder to be ready to receive simulated user input.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePluginPlaceholderTesting)) {
    WebElement element = plugin()->container()->element();
    element.setAttribute("placeholderLoaded", "true");

    scoped_ptr<content::V8ValueConverter> converter(
        content::V8ValueConverter::create());
    base::StringValue value("placeholderLoaded");
    blink::WebSerializedScriptValue message_data =
        blink::WebSerializedScriptValue::serialize(converter->ToV8Value(
            &value, element.document().frame()->mainWorldScriptContext()));

    blink::WebDOMEvent event = element.document().createEvent("MessageEvent");
    blink::WebDOMMessageEvent msg_event = event.to<blink::WebDOMMessageEvent>();
    msg_event.initMessageEvent("message",     // type
                               false,         // canBubble
                               false,         // cancelable
                               message_data,  // data
                               "",            // origin [*]
                               NULL,          // source [*]
                               "");           // lastEventId
    element.dispatchEvent(msg_event);
  }
}

void LoadablePluginPlaceholder::SetPluginInfo(
    const content::WebPluginInfo& plugin_info) {
  plugin_info_ = plugin_info;
}

const content::WebPluginInfo& LoadablePluginPlaceholder::GetPluginInfo() const {
  return plugin_info_;
}

void LoadablePluginPlaceholder::SetIdentifier(const std::string& identifier) {
  identifier_ = identifier;
}

const std::string& LoadablePluginPlaceholder::GetIdentifier() const {
  return identifier_;
}

bool LoadablePluginPlaceholder::LoadingBlocked() const {
  DCHECK(allow_loading_);
  return is_blocked_for_background_tab_ || is_blocked_for_power_saver_poster_ ||
         is_blocked_for_prerendering_;
}

}  // namespace plugins
