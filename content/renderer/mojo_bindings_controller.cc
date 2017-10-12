// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mojo_bindings_controller.h"

#include "base/memory/ptr_util.h"
#include "content/common/view_messages.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/mojo_context_state.h"
#include "gin/per_context_data.h"
#include "third_party/WebKit/public/web/WebContextFeatures.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

namespace content {

namespace {

const char kMojoContextStateKey[] = "MojoContextState";

struct MojoContextStateData : public base::SupportsUserData::Data {
  std::unique_ptr<MojoContextState> state;
};

}  // namespace

MojoBindingsController::MojoBindingsController(RenderFrame* render_frame,
                                               MojoBindingsType bindings_type)
    : RenderFrameObserver(render_frame),
      RenderFrameObserverTracker<MojoBindingsController>(render_frame),
      bindings_type_(bindings_type) {}

MojoBindingsController::~MojoBindingsController() {
}

void MojoBindingsController::CreateContextState() {
  v8::HandleScope handle_scope(blink::MainThreadIsolate());
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  gin::PerContextData* context_data = gin::PerContextData::From(context);
  auto data = base::MakeUnique<MojoContextStateData>();
  data->state.reset(new MojoContextState(frame, context, bindings_type_));
  context_data->SetUserData(kMojoContextStateKey, std::move(data));
}

void MojoBindingsController::DestroyContextState(
    v8::Local<v8::Context> context) {
  gin::PerContextData* context_data = gin::PerContextData::From(context);
  if (!context_data)
    return;
  context_data->RemoveUserData(kMojoContextStateKey);
}

MojoContextState* MojoBindingsController::GetContextState() {
  v8::HandleScope handle_scope(blink::MainThreadIsolate());
  v8::Local<v8::Context> context =
      render_frame()->GetWebFrame()->MainWorldScriptContext();
  gin::PerContextData* context_data = gin::PerContextData::From(context);
  if (!context_data)
    return NULL;
  MojoContextStateData* context_state = static_cast<MojoContextStateData*>(
      context_data->GetUserData(kMojoContextStateKey));
  return context_state ? context_state->state.get() : NULL;
}

void MojoBindingsController::DidCreateScriptContext(
    v8::Local<v8::Context> context,
    int world_id) {
  // NOTE: Layout tests already get this turned on by the RuntimeEnabled feature
  // setting. We avoid manually installing them here for layout tests, because
  // some layout tests (namely at
  // least virtual/stable/webexposed/global-interface-listing.html) may be run
  // with such features explicitly disabled. We do not want to unconditionally
  // install Mojo bindings in such environments.
  //
  // We also only allow these bindings to be installed when creating the main
  // world context.
  if (bindings_type_ != MojoBindingsType::FOR_LAYOUT_TESTS && world_id == 0) {
    blink::WebContextFeatures::EnableMojoJS(context, true);
    // These bindings are only needed by WebUI tests however the browsertest
    // framework does not currently inform the renderer if it is being started
    // by a test. The gin bindings also expose the test interface to WebUI
    // contexts.
    blink::WebContextFeatures::EnableMojoJSTest(context, true);
  }
}

void MojoBindingsController::WillReleaseScriptContext(
    v8::Local<v8::Context> context,
    int world_id) {
  DestroyContextState(context);
}

void MojoBindingsController::RunScriptsAtDocumentStart() {
  CreateContextState();
}

void MojoBindingsController::RunScriptsAtDocumentReady() {
  v8::HandleScope handle_scope(blink::MainThreadIsolate());
  MojoContextState* state = GetContextState();
  if (state)
    state->Run();
}

void MojoBindingsController::DidClearWindowObject() {
  // NOTE: this function may be called early on twice. From the constructor
  // mainWorldScriptContext() may trigger this to be called. If we are created
  // before the page is loaded (which is very likely), then on first load this
  // is called. In the case of the latter we may have already supplied the
  // handle to the context state so that if we destroy now the handle is
  // lost. If this is the result of the first load then the contextstate should
  // be empty and we don't need to destroy it.
  MojoContextState* state = GetContextState();
  if (state && !state->module_added())
    return;

  v8::HandleScope handle_scope(blink::MainThreadIsolate());
  DestroyContextState(render_frame()->GetWebFrame()->MainWorldScriptContext());
}

void MojoBindingsController::OnDestruct() {
  delete this;
}

}  // namespace content
