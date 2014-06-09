// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/test_service/test_service_impl.h"

namespace mojo {
namespace test {

TestServiceImpl::TestServiceImpl() {
}

TestServiceImpl::~TestServiceImpl() {
}

void TestServiceImpl::Ping(const mojo::Callback<void()>& callback) {
  callback.Run();
}

}  // namespace test
}  // namespace mojo
