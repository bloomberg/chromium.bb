// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/apptest/example_client_application.h"

#include "mojo/examples/apptest/example_client_impl.h"

namespace mojo {

ExampleClientApplication::ExampleClientApplication() {}

ExampleClientApplication::~ExampleClientApplication() {}

void ExampleClientApplication::Initialize(ApplicationImpl* app) {}

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new ExampleClientApplication();
}

}  // namespace mojo
