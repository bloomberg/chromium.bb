// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TRAY_ACTION_TEST_TRAY_ACTION_CLIENT_H_
#define ASH_TRAY_ACTION_TEST_TRAY_ACTION_CLIENT_H_

#include "ash/public/interfaces/tray_action.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {

class TestTrayActionClient : public mojom::TrayActionClient {
 public:
  TestTrayActionClient();

  ~TestTrayActionClient() override;

  mojom::TrayActionClientPtr CreateInterfacePtrAndBind();

  void ClearCounts();
  int action_requests_count() const { return action_requests_count_; }
  int action_close_count() const { return action_close_count_; }

  // mojom::TrayActionClient:
  void RequestNewLockScreenNote() override;
  void CloseLockScreenNote() override;

 private:
  mojo::Binding<mojom::TrayActionClient> binding_;

  int action_requests_count_ = 0;
  int action_close_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestTrayActionClient);
};

}  // namespace ash

#endif  // ASH_TRAY_ACTION_TEST_TRAY_ACTION_CLIENT_H_