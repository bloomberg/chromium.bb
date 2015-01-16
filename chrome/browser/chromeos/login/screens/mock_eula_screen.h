// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_EULA_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_EULA_SCREEN_H_

#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/eula_screen.h"
#include "chrome/browser/chromeos/login/screens/eula_view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockEulaScreen : public EulaScreen {
 public:
  MockEulaScreen(BaseScreenDelegate* base_screen_delegate,
                 Delegate* delegate,
                 EulaView* view);
  ~MockEulaScreen() override;
};

class MockEulaView : public EulaView {
 public:
  MockEulaView();
  virtual ~MockEulaView();

  virtual void Bind(EulaModel& model) override;
  virtual void Unbind() override;

  MOCK_METHOD0(PrepareToShow, void());
  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());

  MOCK_CONST_METHOD0(GetName, std::string());

  MOCK_METHOD1(MockBind, void(EulaModel& model));
  MOCK_METHOD0(MockUnbind, void());
  MOCK_METHOD1(OnPasswordFetched, void(const std::string& tpm_password));

 private:
  EulaModel* model_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_EULA_SCREEN_H_
