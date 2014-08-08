// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_TEST_EXAMPLE_SERVICE_IMPL_H_
#define MOJO_EXAMPLES_TEST_EXAMPLE_SERVICE_IMPL_H_

#include "mojo/examples/apptest/example_service.mojom.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {

class ApplicationConnection;

class ExampleServiceImpl : public InterfaceImpl<ExampleService> {
 public:
  ExampleServiceImpl();
  virtual ~ExampleServiceImpl();

 private:
  // InterfaceImpl<ExampleService> overrides.
  virtual void Ping(uint16_t ping_value) MOJO_OVERRIDE;
  virtual void RunCallback(const Callback<void()>& callback) MOJO_OVERRIDE;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ExampleServiceImpl);
};

}  // namespace mojo

#endif  // MOJO_EXAMPLES_TEST_EXAMPLE_SERVICE_IMPL_H_
