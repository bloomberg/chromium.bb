// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_JS_WAITING_CALLBACK_H_
#define MOJO_EDK_JS_WAITING_CALLBACK_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "gin/handle.h"
#include "gin/runner.h"
#include "gin/wrappable.h"
#include "mojo/edk/js/handle.h"
#include "mojo/edk/js/handle_close_observer.h"
#include "mojo/message_pump/handle_watcher.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace edk {
namespace js {

class WaitingCallback : public gin::Wrappable<WaitingCallback>,
                        public HandleCloseObserver {
 public:
  static gin::WrapperInfo kWrapperInfo;

  // Creates a new WaitingCallback.
  static gin::Handle<WaitingCallback> Create(
      v8::Isolate* isolate,
      v8::Handle<v8::Function> callback,
      gin::Handle<HandleWrapper> handle_wrapper,
      MojoHandleSignals signals);

  // Cancels the callback. Does nothing if a callback is not pending. This is
  // implicitly invoked from the destructor but can be explicitly invoked as
  // necessary.
  void Cancel();

 private:
  WaitingCallback(v8::Isolate* isolate,
                  v8::Handle<v8::Function> callback,
                  gin::Handle<HandleWrapper> handle_wrapper);
  ~WaitingCallback() override;

  // Callback from common::HandleWatcher.
  void OnHandleReady(MojoResult result);

  // Invoked by the HandleWrapper if the handle is closed while this wait is
  // still in progress.
  void OnWillCloseHandle() override;

  void RemoveHandleCloseObserver();
  void CallCallback(MojoResult result);

  base::WeakPtr<gin::Runner> runner_;

  common::HandleWatcher handle_watcher_;

  HandleWrapper* handle_wrapper_;
  base::WeakPtrFactory<WaitingCallback> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaitingCallback);
};

}  // namespace js
}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_JS_WAITING_CALLBACK_H_
