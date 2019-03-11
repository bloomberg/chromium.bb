// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_SUPERVISION_TRANSITION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_SUPERVISION_TRANSITION_SCREEN_H_

#include "chrome/browser/chromeos/login/screens/supervision_transition_screen.h"
#include "chrome/browser/chromeos/login/screens/supervision_transition_screen_view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockSupervisionTransitionScreen : public SupervisionTransitionScreen {
 public:
  MockSupervisionTransitionScreen(SupervisionTransitionScreenView* view,
                                  const base::RepeatingClosure& exit_callback);
  virtual ~MockSupervisionTransitionScreen();

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());

  void ExitScreen();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSupervisionTransitionScreen);
};

class MockSupervisionTransitionScreenView
    : public SupervisionTransitionScreenView {
 public:
  MockSupervisionTransitionScreenView();
  virtual ~MockSupervisionTransitionScreenView();

  void Bind(SupervisionTransitionScreen* screen) override;
  void Unbind() override;

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD1(MockBind, void(SupervisionTransitionScreen* screen));
  MOCK_METHOD0(MockUnbind, void());

 private:
  SupervisionTransitionScreen* screen_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(MockSupervisionTransitionScreenView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_SUPERVISION_TRANSITION_SCREEN_H_
