// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_BROWSER_INTERFACE_H_
#define CHROME_BROWSER_VR_TEST_MOCK_BROWSER_INTERFACE_H_

#include "base/macros.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace vr {

class MockBrowserInterface : public UiBrowserInterface {
 public:
  MockBrowserInterface();
  ~MockBrowserInterface() override;

  MOCK_METHOD0(ExitPresent, void());
  MOCK_METHOD0(ExitFullscreen, void());
  MOCK_METHOD0(NavigateBack, void());
  MOCK_METHOD0(ExitCct, void());
  MOCK_METHOD1(OnUnsupportedMode, void(UiUnsupportedMode mode));
  MOCK_METHOD2(OnExitVrPromptResult,
               void(UiUnsupportedMode reason, ExitVrPromptChoice choice));
  MOCK_METHOD1(OnContentScreenBoundsChanged, void(const gfx::SizeF& bounds));
  MOCK_METHOD1(SetVoiceSearchActive, void(bool active));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBrowserInterface);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_MOCK_BROWSER_INTERFACE_H_
