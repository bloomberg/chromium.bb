// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_BROWSER_UI_INTERFACE_H_
#define CHROME_BROWSER_VR_TEST_MOCK_BROWSER_UI_INTERFACE_H_

#include "base/macros.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/model/toolbar_state.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace vr {

class MockBrowserUiInterface : public BrowserUiInterface {
 public:
  MockBrowserUiInterface();
  ~MockBrowserUiInterface() override;

  MOCK_METHOD2(SetWebVrMode, void(bool enabled, bool show_toast));
  MOCK_METHOD1(SetFullscreen, void(bool enabled));
  MOCK_METHOD1(SetToolbarState, void(const ToolbarState& state));
  MOCK_METHOD1(SetIncognito, void(bool enabled));
  MOCK_METHOD1(SetLoading, void(bool loading));
  MOCK_METHOD1(SetLoadProgress, void(float progress));
  MOCK_METHOD0(SetIsExiting, void());
  MOCK_METHOD2(SetHistoryButtonsEnabled,
               void(bool can_go_back, bool can_go_forward));
  MOCK_METHOD1(SetVideoCaptureEnabled, void(bool enabled));
  MOCK_METHOD1(SetScreenCaptureEnabled, void(bool enabled));
  MOCK_METHOD1(SetAudioCaptureEnabled, void(bool enabled));
  MOCK_METHOD1(SetBluetoothConnected, void(bool enabled));
  MOCK_METHOD1(SetLocationAccessEnabled, void(bool enabled));
  MOCK_METHOD1(ShowExitVrPrompt, void(UiUnsupportedMode reason));
  MOCK_METHOD1(SetSpeechRecognitionEnabled, void(bool enabled));
  MOCK_METHOD1(SetRecognitionResult, void(const base::string16& result));
  MOCK_METHOD1(OnSpeechRecognitionStateChanged, void(int new_state));
  void SetOmniboxSuggestions(std::unique_ptr<OmniboxSuggestions> suggestions) {}
  void OnAssetsLoaded(AssetsLoadStatus status,
                      std::unique_ptr<Assets> assets,
                      const base::Version& component_version) {}
  MOCK_METHOD0(OnAssetsUnavailable, void());

  MOCK_METHOD1(ShowSoftInput, void(bool));
  MOCK_METHOD4(UpdateWebInputIndices, void(int, int, int, int));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBrowserUiInterface);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_MOCK_BROWSER_UI_INTERFACE_H_
