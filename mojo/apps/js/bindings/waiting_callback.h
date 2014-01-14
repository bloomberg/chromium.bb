// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPS_JS_BINDINGS_WAITING_CALLBACK_H_
#define MOJO_APPS_JS_BINDINGS_WAITING_CALLBACK_H_

#include "gin/handle.h"
#include "gin/runner.h"
#include "gin/wrappable.h"
#include "mojo/public/system/async_waiter.h"

namespace mojo {
namespace js {

class WaitingCallback : public gin::Wrappable<WaitingCallback> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static gin::Handle<WaitingCallback> Create(
      v8::Isolate* isolate, v8::Handle<v8::Function> callback);

  MojoAsyncWaitID wait_id() const {
    return wait_id_;
  }

  void set_wait_id(MojoAsyncWaitID wait_id) {
    wait_id_ = wait_id;
  }

  // MojoAsyncWaitCallback
  static void CallOnHandleReady(void* closure, MojoResult result);

 private:
  WaitingCallback(v8::Isolate* isolate, v8::Handle<v8::Function> callback);
  virtual ~WaitingCallback();

  void OnHandleReady(MojoResult result);

  base::WeakPtr<gin::Runner> runner_;
  MojoAsyncWaitID wait_id_;

  DISALLOW_COPY_AND_ASSIGN(WaitingCallback);
};

}  // namespace js
}  // namespace mojo

#endif  // MOJO_APPS_JS_BINDINGS_WAITING_CALLBACK_H_
