// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/tray_action/test_tray_action_client.h"

#include "mojo/public/cpp/bindings/interface_request.h"

namespace ash {

TestTrayActionClient::TestTrayActionClient() : binding_(this) {}

TestTrayActionClient::~TestTrayActionClient() = default;

void TestTrayActionClient::ClearCounts() {
  action_requests_count_ = 0;
  action_close_count_ = 0;
}

void TestTrayActionClient::RequestNewLockScreenNote() {
  action_requests_count_++;
}

void TestTrayActionClient::CloseLockScreenNote() {
  action_close_count_++;
}

mojom::TrayActionClientPtr TestTrayActionClient::CreateInterfacePtrAndBind() {
  mojom::TrayActionClientPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

}  // namespace ash