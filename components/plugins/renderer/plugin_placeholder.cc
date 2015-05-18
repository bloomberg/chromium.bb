// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plugins/renderer/plugin_placeholder.h"

#include "base/strings/string_util.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "gin/object_template_builder.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/re2/re2/re2.h"

namespace plugins {

// The placeholder is loaded in normal web renderer processes, so it should not
// have a chrome:// scheme that might let it be confused with a WebUI page.
const char kPluginPlaceholderDataURL[] = "data:text/html,pluginplaceholderdata";

gin::WrapperInfo PluginPlaceholder::kWrapperInfo = {gin::kEmbedderNativeGin};

PluginPlaceholder::PluginPlaceholder(content::RenderFrame* render_frame,
                                     blink::WebLocalFrame* frame,
                                     const blink::WebPluginParams& params,
                                     const std::string& html_data)
    : content::RenderFrameObserver(render_frame),
      frame_(frame),
      plugin_params_(params),
      plugin_(WebViewPlugin::Create(this,
                                    render_frame->GetWebkitPreferences(),
                                    html_data,
                                    GURL(kPluginPlaceholderDataURL))),
      hidden_(false) {
}

PluginPlaceholder::~PluginPlaceholder() {}

const blink::WebPluginParams& PluginPlaceholder::GetPluginParams() const {
  return plugin_params_;
}

void PluginPlaceholder::BindWebFrame(blink::WebFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  DCHECK(!context.IsEmpty());

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "plugin"),
              gin::CreateHandle(isolate, this).ToV8());
}

void PluginPlaceholder::ShowContextMenu(const blink::WebMouseEvent& event) {
  // Does nothing by default. Will be overridden if a specific browser wants
  // a context menu.
  return;
}

void PluginPlaceholder::PluginDestroyed() {
  plugin_ = NULL;
}

v8::Local<v8::Object> PluginPlaceholder::GetV8ScriptableObject(
    v8::Isolate* isolate) const {
  return v8::Local<v8::Object>();
}

gin::ObjectTemplateBuilder PluginPlaceholder::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<PluginPlaceholder>::GetObjectTemplateBuilder(isolate)
      .SetMethod("hide", &PluginPlaceholder::HideCallback);
}

void PluginPlaceholder::HidePlugin() {
  hidden_ = true;
  if (!plugin())
    return;
  blink::WebPluginContainer* container = plugin()->container();
  blink::WebElement element = container->element();
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
    blink::WebNode parent = element;
    while (!parent.parentNode().isNull()) {
      parent = parent.parentNode();
      if (!parent.isElementNode())
        continue;
      element = parent.toConst<blink::WebElement>();
      if (element.hasAttribute("style")) {
        std::string style_str = element.getAttribute("style").utf8();
        if (RE2::PartialMatch(style_str, width_str) &&
            RE2::PartialMatch(style_str, height_str))
          element.setAttribute("style", "display: none;");
      }
    }
  }
}

void PluginPlaceholder::OnDestruct() {
  frame_ = NULL;
}

void PluginPlaceholder::HideCallback() {
  content::RenderThread::Get()->RecordAction(
      base::UserMetricsAction("Plugin_Hide_Click"));
  HidePlugin();
}

blink::WebLocalFrame* PluginPlaceholder::GetFrame() { return frame_; }

}  // namespace plugins
