// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/renderer/shell_content_renderer_client.h"

#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extensions_client.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_helper.h"
#include "extensions/shell/common/shell_extensions_client.h"
#include "extensions/shell/renderer/shell_dispatcher_delegate.h"
#include "extensions/shell/renderer/shell_extensions_renderer_client.h"
#include "extensions/shell/renderer/shell_renderer_main_delegate.h"

using blink::WebFrame;
using blink::WebString;
using content::RenderThread;

namespace extensions {

namespace {

// TODO: promote ExtensionFrameHelper to a common place and share with this.
class ShellFrameHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<ShellFrameHelper> {
 public:
  ShellFrameHelper(content::RenderFrame* render_frame,
                   Dispatcher* extension_dispatcher);
  virtual ~ShellFrameHelper();

  // RenderFrameObserver implementation.
  virtual void WillReleaseScriptContext(v8::Handle<v8::Context>,
                                        int world_id) OVERRIDE;

 private:
  Dispatcher* extension_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ShellFrameHelper);
};

ShellFrameHelper::ShellFrameHelper(content::RenderFrame* render_frame,
                                   Dispatcher* extension_dispatcher)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<ShellFrameHelper>(render_frame),
      extension_dispatcher_(extension_dispatcher) {
}

ShellFrameHelper::~ShellFrameHelper() {
}

void ShellFrameHelper::WillReleaseScriptContext(v8::Handle<v8::Context> context,
                                                int world_id) {
  extension_dispatcher_->WillReleaseScriptContext(
      render_frame()->GetWebFrame(), context, world_id);
}

}  // namespace

ShellContentRendererClient::ShellContentRendererClient(
    scoped_ptr<ShellRendererMainDelegate> delegate)
    : delegate_(delegate.Pass()) {
}

ShellContentRendererClient::~ShellContentRendererClient() {
}

void ShellContentRendererClient::RenderThreadStarted() {
  RenderThread* thread = RenderThread::Get();

  extensions_client_.reset(new ShellExtensionsClient);
  ExtensionsClient::Set(extensions_client_.get());

  extensions_renderer_client_.reset(new ShellExtensionsRendererClient);
  ExtensionsRendererClient::Set(extensions_renderer_client_.get());

  extension_dispatcher_delegate_.reset(new ShellDispatcherDelegate());

  // Must be initialized after ExtensionsRendererClient.
  extension_dispatcher_.reset(
      new Dispatcher(extension_dispatcher_delegate_.get()));
  thread->AddObserver(extension_dispatcher_.get());

  // TODO(jamescook): Init WebSecurityPolicy for chrome-extension: schemes.
  // See ChromeContentRendererClient for details.
  if (delegate_)
    delegate_->OnThreadStarted(thread);
}

void ShellContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  // ShellFrameHelper destroyes itself when the RenderFrame is destroyed.
  new ShellFrameHelper(render_frame, extension_dispatcher_.get());
}

void ShellContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  new ExtensionHelper(render_view, extension_dispatcher_.get());
  if (delegate_)
    delegate_->OnViewCreated(render_view);
}

bool ShellContentRendererClient::WillSendRequest(
    blink::WebFrame* frame,
    content::PageTransition transition_type,
    const GURL& url,
    const GURL& first_party_for_cookies,
    GURL* new_url) {
  // TODO(jamescook): Cause an error for bad extension scheme requests?
  return false;
}

void ShellContentRendererClient::DidCreateScriptContext(
    WebFrame* frame,
    v8::Handle<v8::Context> context,
    int extension_group,
    int world_id) {
  extension_dispatcher_->DidCreateScriptContext(
      frame, context, extension_group, world_id);
}

bool ShellContentRendererClient::ShouldEnableSiteIsolationPolicy() const {
  // Extension renderers don't need site isolation.
  return false;
}

}  // namespace extensions
