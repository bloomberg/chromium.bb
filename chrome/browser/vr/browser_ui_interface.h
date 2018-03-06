// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_BROWSER_UI_INTERFACE_H_
#define CHROME_BROWSER_VR_BROWSER_UI_INTERFACE_H_

#include "chrome/browser/vr/assets_load_status.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "components/security_state/core/security_state.h"

namespace base {
class Version;
}  // namespace base

namespace vr {

struct Assets;
struct OmniboxSuggestions;
struct ToolbarState;

// The browser communicates state changes to the VR UI via this interface.
class BrowserUiInterface {
 public:
  virtual ~BrowserUiInterface() {}

  virtual void SetWebVrMode(bool enabled, bool show_toast) = 0;
  virtual void SetFullscreen(bool enabled) = 0;
  virtual void SetToolbarState(const ToolbarState& state) = 0;
  virtual void SetIncognito(bool enabled) = 0;
  virtual void SetLoading(bool loading) = 0;
  virtual void SetLoadProgress(float progress) = 0;
  virtual void SetIsExiting() = 0;
  virtual void SetHistoryButtonsEnabled(bool can_go_back,
                                        bool can_go_forward) = 0;
  virtual void SetVideoCaptureEnabled(bool enabled) = 0;
  virtual void SetScreenCaptureEnabled(bool enabled) = 0;
  virtual void SetAudioCaptureEnabled(bool enabled) = 0;
  virtual void SetBluetoothConnected(bool enabled) = 0;
  virtual void SetLocationAccessEnabled(bool enabled) = 0;
  virtual void ShowExitVrPrompt(UiUnsupportedMode reason) = 0;
  virtual void SetSpeechRecognitionEnabled(bool enabled) = 0;
  virtual void SetRecognitionResult(const base::string16& result) = 0;
  virtual void OnSpeechRecognitionStateChanged(int new_state) = 0;
  virtual void SetOmniboxSuggestions(
      std::unique_ptr<OmniboxSuggestions> suggestions) = 0;
  virtual void OnAssetsLoaded(AssetsLoadStatus status,
                              std::unique_ptr<Assets> assets,
                              const base::Version& component_version) = 0;
  virtual void OnAssetsUnavailable() = 0;

  // Web contents text input related.
  virtual void ShowSoftInput(bool show) = 0;
  virtual void UpdateWebInputIndices(int selection_start,
                                     int selection_end,
                                     int composition_start,
                                     int composition_end) = 0;

  // Tab handling.
  virtual void AppendToTabList(bool incognito,
                               int id,
                               const base::string16& title) {}
  virtual void FlushTabList() {}
  virtual void UpdateTab(bool incognito, int id, const std::string& title) {}
  virtual void RemoveTab(bool incognito, int id) {}
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_BROWSER_UI_INTERFACE_H_
