// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPS_JS_BINDINGS_WAITING_CALLBACK_H_
#define MOJO_APPS_JS_BINDINGS_WAITING_CALLBACK_H_

#include "gin/handle.h"
#include "gin/runner.h"
#include "gin/wrappable.h"
#include "mojo/public/bindings/lib/bindings_support.h"

namespace mojo {
namespace js {

class WaitingCallback : public gin::Wrappable,
                        public BindingsSupport::AsyncWaitCallback {
 public:
  static gin::Handle<WaitingCallback> Create(
      v8::Isolate* isolate, v8::Handle<v8::Function> callback);

  static gin::WrapperInfo kWrapperInfo;
  virtual gin::WrapperInfo* GetWrapperInfo() OVERRIDE;
  static void EnsureRegistered(v8::Isolate* isolate);

  BindingsSupport::AsyncWaitID wait_id() const {
    return wait_id_;
  }

  void set_wait_id(BindingsSupport::AsyncWaitID wait_id) {
    wait_id_ = wait_id;
  }

 private:
  WaitingCallback(v8::Isolate* isolate, v8::Handle<v8::Function> callback);
  virtual ~WaitingCallback();

  virtual void OnHandleReady(MojoResult result) OVERRIDE;

  base::WeakPtr<gin::Runner> runner_;
  BindingsSupport::AsyncWaitID wait_id_;

  DISALLOW_COPY_AND_ASSIGN(WaitingCallback);
};

}  // namespace js
}  // namespace mojo

#endif  // MOJO_APPS_JS_BINDINGS_WAITING_CALLBACK_H_
