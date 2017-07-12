// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_RENDERER_HEADLESS_TAB_SOCKET_BINDINGS_H_
#define HEADLESS_LIB_RENDERER_HEADLESS_TAB_SOCKET_BINDINGS_H_

#include "content/public/renderer/render_frame.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "headless/lib/tab_socket.mojom.h"
#include "third_party/WebKit/public/web/WebScriptExecutionCallback.h"

namespace headless {
class HeadlessRenderFrameControllerImpl;

class HeadlessTabSocketBindings
    : public gin::Wrappable<HeadlessTabSocketBindings>,
      public blink::WebScriptExecutionCallback {
 public:
  HeadlessTabSocketBindings(
      HeadlessRenderFrameControllerImpl* parent_controller,
      content::RenderFrame* render_frame,
      v8::Local<v8::Context> context,
      int world_id);

  ~HeadlessTabSocketBindings() override;

  // Add TabSocket bindings to |context_|.
  bool InitializeTabSocketBindings();

  void OnMessageFromEmbedder(const std::string& message);

  // gin::WrappableBase implementation:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) override;

  static gin::WrapperInfo kWrapperInfo;

  int world_id() const { return world_id_; }

 private:
  void SendImpl(const std::string& message);

  v8::Local<v8::Value> GetOnMessage() { return GetOnMessageCallback(); }

  void SetOnMessage(v8::Local<v8::Function> callback);

  v8::Local<v8::Function> GetOnMessageCallback();

  HeadlessRenderFrameControllerImpl* const parent_controller_;  // NOT OWNED
  content::RenderFrame* const render_frame_;                    // NOT OWNED
  const v8::UniquePersistent<v8::Context> context_;
  const int world_id_;
  bool installed_;
  std::list<std::string> pending_messages_;
  v8::UniquePersistent<v8::Function> on_message_callback_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_RENDERER_HEADLESS_TAB_SOCKET_BINDINGS_H_
