// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser/blimp_browser_main_parts.h"

#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/engine/browser/blimp_browser_context.h"
#include "blimp/engine/browser/blimp_engine_session.h"
#include "blimp/net/blimp_connection.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/main_function_params.h"
#include "net/base/net_module.h"
#include "net/log/net_log.h"

#if defined(USE_X11)
#include "ui/base/ime/input_method_initializer.h"
#endif

namespace blimp {
namespace engine {

BlimpBrowserMainParts::BlimpBrowserMainParts(
    const content::MainFunctionParams& parameters) {}

BlimpBrowserMainParts::~BlimpBrowserMainParts() {}

void BlimpBrowserMainParts::PreEarlyInitialization() {
#if defined(USE_X11)
  // TODO(haibinlu): Rename the method below. crbug/548330.
  ui::InitializeInputMethodForTesting();
#endif
}

void BlimpBrowserMainParts::PreMainMessageLoopRun() {
  net_log_.reset(new net::NetLog());
  scoped_ptr<BlimpBrowserContext> browser_context(
      new BlimpBrowserContext(false, net_log_.get()));
  engine_session_.reset(new BlimpEngineSession(browser_context.Pass()));
  engine_session_->Initialize();

  // TODO(haibinlu): Create EngineConnectionManager to accept new connections.
  // TODO(haibinlu): Remove these test messages and switch to using the
  // MessageDispatcher for incoming messages.
  scoped_ptr<BlimpMessage> message(new BlimpMessage);
  message->set_type(BlimpMessage::CONTROL);
  message->mutable_control()->set_type(ControlMessage::CREATE_TAB);
  engine_session_->ProcessMessage(message.Pass(), net::CompletionCallback());
  message.reset(new BlimpMessage);
  message->set_type(BlimpMessage::NAVIGATION);
  message->mutable_navigation()->set_type(NavigationMessage::LOAD_URL);
  message->mutable_navigation()->mutable_load_url()->set_url(
      "https://www.google.com/");
  engine_session_->ProcessMessage(message.Pass(), net::CompletionCallback());
}

void BlimpBrowserMainParts::PostMainMessageLoopRun() {
  engine_session_.reset();
}

BlimpBrowserContext* BlimpBrowserMainParts::GetBrowserContext() {
  return engine_session_->browser_context();
}

}  // namespace engine
}  // namespace blimp
