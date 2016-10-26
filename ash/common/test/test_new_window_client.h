// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_TEST_TEST_NEW_WINDOW_CLIENT_H_
#define ASH_COMMON_TEST_TEST_NEW_WINDOW_CLIENT_H_

#include "ash/public/interfaces/new_window.mojom.h"

namespace ash {

// A mojom::NewWindowClient which doesn't try to access a remote mojo component
// for testing purpose.
class TestNewWindowClient : public mojom::NewWindowClient {
 public:
  TestNewWindowClient();
  ~TestNewWindowClient() override;

  // NewWindowClient:
  void NewTab() override;
  void NewWindow(bool incognito) override;
  void OpenFileManager() override;
  void OpenCrosh() override;
  void OpenGetHelp() override;
  void RestoreTab() override;
  void ShowKeyboardOverlay() override;
  void ShowTaskManager() override;
  void OpenFeedbackPage() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestNewWindowClient);
};

}  // namespace ash

#endif  // ASH_COMMON_TEST_TEST_NEW_WINDOW_CLIENT_H_
