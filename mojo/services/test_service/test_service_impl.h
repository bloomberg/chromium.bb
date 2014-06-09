// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/services/test_service/test_service.mojom.h"

namespace mojo {
namespace test {

class TestServiceImpl : public InterfaceImpl<ITestService> {
 public:
  TestServiceImpl();
  virtual ~TestServiceImpl();

  // |ITestService| methods:
  virtual void Ping(const mojo::Callback<void()>& callback) MOJO_OVERRIDE;

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(TestServiceImpl);
};

}  // namespace test
}  // namespace mojo

