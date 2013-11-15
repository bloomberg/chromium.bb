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
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/re2/re2/re2.h"

using content::RenderThread;
using blink::WebElement;
using blink::WebFrame;
using blink::WebMouseEvent;
using blink::WebNode;
using blink::WebPlugin;
using blink::WebPluginContainer;
using blink::WebPluginParams;
using blink::WebScriptSource;
using blink::WebURLRequest;

namespace plugins {

namespace {

// This class is supposed to be stored in an v8::External. It holds a
// base::Closure and will delete itself as soon as the v8::External is garbage
// collected.
class V8ClosureWrapper {
 public:
  explicit V8ClosureWrapper(const base::Closure& closure) : closure_(closure) {}

  void SetExternal(v8::Isolate* isolate, v8::Handle<v8::External> external) {
    DCHECK(external_.IsEmpty());
    external_.Reset(isolate, external);
    external_.SetWeak(this, &V8ClosureWrapper::WeakCallback);
  }

  void Run() {
    closure_.Run();
  }

 private:
  static void WeakCallback(
      const v8::WeakCallbackData<v8::External, V8ClosureWrapper>& data) {
    V8ClosureWrapper* info = data.GetParameter();
    info->external_.Reset();
    delete info;
  }

  ~V8ClosureWrapper() {}

  base::Closure closure_;
  v8::Persistent<v8::External> external_;

  DISALLOW_COPY_AND_ASSIGN(V8ClosureWrapper);
};

void RunV8ClosureWrapper(const v8::FunctionCallbackInfo<v8::Value>& args) {
  V8ClosureWrapper* wrapper = reinterpret_cast<V8ClosureWrapper*>(
      v8::External::Cast(*args.Data())->Value());
  wrapper->Run();
}

}  // namespace

PluginPlaceholder::PluginPlaceholder(content::RenderView* render_view,
                                     WebFrame* frame,
                                     const WebPluginParams& params,
                                     const std::string& html_data,
                                     GURL placeholderDataUrl)
    : content::RenderViewObserver(render_view),
      frame_(frame),
      plugin_params_(params),
      plugin_(WebViewPlugin::Create(this,
                                    render_view->GetWebkitPreferences(),
                                    html_data,
                                    placeholderDataUrl)),
      is_blocked_for_prerendering_(false),
      allow_loading_(false),
      hidden_(false),
      finished_loading_(false),
      weak_factory_(this) {
  RegisterCallback(
      "load",
      base::Bind(&PluginPlaceholder::LoadCallback, weak_factory_.GetWeakPtr()));
  RegisterCallback(
      "hide",
      base::Bind(&PluginPlaceholder::HideCallback, weak_factory_.GetWeakPtr()));
  RegisterCallback("didFinishLoading",
                   base::Bind(&PluginPlaceholder::DidFinishLoadingCallback,
                              weak_factory_.GetWeakPtr()));
}

PluginPlaceholder::~PluginPlaceholder() {}

void PluginPlaceholder::RegisterCallback(const std::string& callback_name,
                                         const base::Closure& callback) {
  DCHECK(callbacks_.find(callback_name) == callbacks_.end());
  callbacks_[callback_name] = callback;
}

void PluginPlaceholder::BindWebFrame(WebFrame* frame) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context = frame->mainWorldScriptContext();

  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  v8::Handle<v8::FunctionTemplate> plugin_template =
      v8::FunctionTemplate::New();
  v8::Handle<v8::Template> prototype = plugin_template->PrototypeTemplate();

  for (std::map<std::string, base::Closure>::const_iterator callback =
           callbacks_.begin();
       callback != callbacks_.end();
       ++callback) {
    V8ClosureWrapper* wrapper = new V8ClosureWrapper(callback->second);
    v8::Handle<v8::External> wrapper_holder = v8::External::New(wrapper);
    wrapper->SetExternal(isolate, wrapper_holder);
    prototype->Set(
        v8::String::New(callback->first.c_str()),
        v8::FunctionTemplate::New(RunV8ClosureWrapper, wrapper_holder));
  }

  v8::Handle<v8::Object> global = context->Global();
  global->Set(v8::String::New("plugin"),
              plugin_template->GetFunction()->NewInstance());
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

  // The plug-in has been removed from the page. Destroy the old plug-in
  // (which will destroy us).
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
    std::string height_str("height:[\\s]*");
    height_str += element.getAttribute("height").utf8().data();
    if (EndsWith(height_str, "px", false)) {
      height_str = height_str.substr(0, height_str.length() - 2);
    }
    TrimWhitespace(height_str, TRIM_TRAILING, &height_str);
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

void PluginPlaceholder::WillDestroyPlugin() { delete this; }

void PluginPlaceholder::SetMessage(const string16& message) {
  message_ = message;
  if (finished_loading_)
    UpdateMessage();
}

void PluginPlaceholder::UpdateMessage() {
  std::string script =
      "window.setMessage(" + base::GetDoubleQuotedJson(message_) + ")";
  plugin_->web_view()->mainFrame()->executeScript(
      WebScriptSource(ASCIIToUTF16(script)));
}

void PluginPlaceholder::ShowContextMenu(const WebMouseEvent& event) {
  // Does nothing by default. Will be overridden if a specific browser wants
  // a context menu.
  return;
}

void PluginPlaceholder::OnLoadBlockedPlugins(const std::string& identifier) {
  if (!identifier.empty() && identifier != identifier_)
    return;

  RenderThread::Get()->RecordUserMetrics("Plugin_Load_UI");
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
  if (!allow_loading_) {
    NOTREACHED();
    return;
  }

  // TODO(mmenke):  In the case of prerendering, feed into
  //                ChromeContentRendererClient::CreatePlugin instead, to
  //                reduce the chance of future regressions.
  WebPlugin* plugin =
      render_view()->CreatePlugin(frame_, plugin_info_, plugin_params_);
  ReplacePlugin(plugin);
}

void PluginPlaceholder::LoadCallback() {
  RenderThread::Get()->RecordUserMetrics("Plugin_Load_Click");
  LoadPlugin();
}

void PluginPlaceholder::HideCallback() {
  RenderThread::Get()->RecordUserMetrics("Plugin_Hide_Click");
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

blink::WebFrame* PluginPlaceholder::GetFrame() { return frame_; }

const blink::WebPluginParams& PluginPlaceholder::GetPluginParams() const {
  return plugin_params_;
}

}  // namespace plugins
