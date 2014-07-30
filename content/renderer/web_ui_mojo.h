// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_UI_MOJO_H_
#define CONTENT_RENDERER_WEB_UI_MOJO_H_

#include <string>

#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "mojo/public/cpp/system/core.h"

namespace gin {
class PerContextData;
}

namespace content {

class WebUIMojoContextState;

// WebUIMojo is responsible for enabling the renderer side of mojo bindings.
// It creates (and destroys) a WebUIMojoContextState at the appropriate times
// and handles the necessary browser messages. WebUIMojo destroys itself when
// the RendererView it is created with is destroyed.
class WebUIMojo
    : public RenderViewObserver,
      public RenderViewObserverTracker<WebUIMojo> {
 public:
  explicit WebUIMojo(RenderView* render_view);

 private:
  class MainFrameObserver : public RenderFrameObserver {
   public:
    explicit MainFrameObserver(WebUIMojo* web_ui_mojo);
    virtual ~MainFrameObserver();

    // RenderFrameObserver overrides:
    virtual void WillReleaseScriptContext(v8::Handle<v8::Context> context,
                                          int world_id) OVERRIDE;
    virtual void DidFinishDocumentLoad() OVERRIDE;

   private:
    WebUIMojo* web_ui_mojo_;

    DISALLOW_COPY_AND_ASSIGN(MainFrameObserver);
  };

  virtual ~WebUIMojo();

  void CreateContextState();
  void DestroyContextState(v8::Handle<v8::Context> context);

  // Invoked when the frame finishes loading. Invokes Run() on the
  // WebUIMojoContextState.
  void OnDidFinishDocumentLoad();

  WebUIMojoContextState* GetContextState();

  // RenderViewObserver overrides:
  virtual void DidClearWindowObject(blink::WebLocalFrame* frame) OVERRIDE;

  MainFrameObserver main_frame_observer_;

  DISALLOW_COPY_AND_ASSIGN(WebUIMojo);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEB_UI_MOJO_H_
