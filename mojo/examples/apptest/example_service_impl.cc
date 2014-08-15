// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/apptest/example_service_impl.h"

#include "mojo/public/cpp/utility/run_loop.h"

namespace mojo {

ExampleServiceImpl::ExampleServiceImpl() {}

ExampleServiceImpl::~ExampleServiceImpl() {}

void ExampleServiceImpl::Ping(uint16_t ping_value) {
  client()->Pong(ping_value);
  RunLoop::current()->Quit();
}

void ExampleServiceImpl::RunCallback(const Callback<void()>& callback) {
  callback.Run();
  RunLoop::current()->Quit();
}

}  // namespace mojo
