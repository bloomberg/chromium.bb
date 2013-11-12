// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/js/runner_delegate.h"

#include "mojo/public/bindings/js/mojo.h"

namespace mojo {
namespace js {

RunnerDelegate::RunnerDelegate() {
}

RunnerDelegate::~RunnerDelegate() {
}

v8::Handle<v8::Object> RunnerDelegate::CreateRootObject(gin::Runner* runner) {
  return GetMojoTemplate(runner->isolate())->NewInstance();
}

}  // namespace js
}  // namespace mojo
