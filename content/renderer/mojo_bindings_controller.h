// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOJO_BINDINGS_CONTROLLER_H_
#define CONTENT_RENDERER_MOJO_BINDINGS_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "mojo/public/cpp/system/core.h"

namespace gin {
class PerContextData;
}

namespace content {

class MojoContextState;

// MojoBindingsController is responsible for enabling the renderer side of mojo
// bindings. It creates (and destroys) a MojoContextState at the appropriate
// times and handles the necessary browser messages. MojoBindingsController
// destroys itself when the RendererView it is created with is destroyed.
class MojoBindingsController
    : public RenderViewObserver,
      public RenderViewObserverTracker<MojoBindingsController> {
 public:
  explicit MojoBindingsController(RenderView* render_view);

 private:
  class MainFrameObserver : public RenderFrameObserver {
   public:
    explicit MainFrameObserver(MojoBindingsController* web_ui_mojo);
    ~MainFrameObserver() override;

    // RenderFrameObserver overrides:
    void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                  int world_id) override;
    void DidFinishDocumentLoad() override;
    // MainFrameObserver is inline owned by MojoBindingsController and should
    // not be destroyed when the main RenderFrame is deleted. Overriding the
    // OnDestruct method allows this object to remain alive and be cleaned
    // up as part of MojoBindingsController deletion.
    void OnDestruct() override;

   private:
    MojoBindingsController* mojo_bindings_controller_;

    DISALLOW_COPY_AND_ASSIGN(MainFrameObserver);
  };

  ~MojoBindingsController() override;

  void CreateContextState();
  void DestroyContextState(v8::Local<v8::Context> context);

  // Invoked when the frame finishes loading. Invokes Run() on the
  // MojoContextState.
  void OnDidFinishDocumentLoad();

  MojoContextState* GetContextState();

  // RenderViewObserver overrides:
  void DidCreateDocumentElement(blink::WebLocalFrame* frame) override;
  void DidClearWindowObject(blink::WebLocalFrame* frame) override;

  MainFrameObserver main_frame_observer_;

  DISALLOW_COPY_AND_ASSIGN(MojoBindingsController);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOJO_BINDINGS_CONTROLLER_H_
