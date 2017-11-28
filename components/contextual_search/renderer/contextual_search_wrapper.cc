// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/renderer/contextual_search_wrapper.h"

#include "base/strings/string_util.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "gin/arguments.h"
#include "gin/object_template_builder.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "v8/include/v8.h"

namespace {

static const char kContextualSearchObjectName[] = "contextualSearch";
static const char kSetCaptionMethodName[] = "setCaption";

}  // namespace

namespace contextual_search {

gin::WrapperInfo ContextualSearchWrapper::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
void ContextualSearchWrapper::Install(content::RenderFrame* render_frame) {
  // NOTE: Installing new v8 functions that can access Chrome native code
  // requires a security review!  We did an exhaustive search for a better
  // way to implement a communication channel between the page and Chrome,
  // but found nothing better.
  // TODO(donnd): use a better communication channel once that becomes
  // available, e.g. navigator.connect API. See crbug.com/541683.
  // TODO(donnd): refactor some of this boilerplate into a reusable
  // method.  This was cribbed from MemoryBenchmarkingExtension.
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);
  gin::Handle<ContextualSearchWrapper> wrapper =
      gin::CreateHandle(isolate, new ContextualSearchWrapper(render_frame));
  if (wrapper.IsEmpty())
    return;

  v8::Local<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context->Global());
  chrome->Set(gin::StringToV8(isolate, kContextualSearchObjectName),
              wrapper.ToV8());
}

ContextualSearchWrapper::ContextualSearchWrapper(
    content::RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {}

ContextualSearchWrapper::~ContextualSearchWrapper() {}

gin::ObjectTemplateBuilder ContextualSearchWrapper::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<ContextualSearchWrapper>::GetObjectTemplateBuilder(
             isolate)
      .SetMethod(kSetCaptionMethodName, &ContextualSearchWrapper::SetCaption);
}

bool ContextualSearchWrapper::EnsureServiceConnected() {
  if (render_frame() && (!contextual_search_js_api_service_ ||
      !contextual_search_js_api_service_.is_bound())) {
    render_frame()->GetRemoteInterfaces()->GetInterface(
        &contextual_search_js_api_service_);
    return true;
  }
  return false;
}

void ContextualSearchWrapper::OnDestruct() {}

void ContextualSearchWrapper::SetCaption(const std::string& caption,
                                         bool does_answer) {
  if (EnsureServiceConnected()) {
    contextual_search_js_api_service_->HandleSetCaption(caption, does_answer);
  }
}

}  // namespace contextual_search
