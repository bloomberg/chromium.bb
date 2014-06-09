// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/services/test_service/test_service_impl.h"

namespace mojo {
namespace test {
namespace {

class TestServiceApplication : public Application {
 public:
  TestServiceApplication() {}
  virtual ~TestServiceApplication() {}

  virtual void Initialize() MOJO_OVERRIDE {
    AddService<TestServiceImpl>();
  }

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(TestServiceApplication);
};

}  // namespace
}  // namespace test

// static
Application* Application::Create() {
  return new mojo::test::TestServiceApplication();
}

}  // namespace mojo
