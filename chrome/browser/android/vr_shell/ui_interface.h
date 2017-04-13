// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_INTERFACE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_INTERFACE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/values.h"

class GURL;

namespace vr_shell {

// This class manages the communication of browser state from VR shell to the
// HTML UI. State information is asynchronous and unidirectional.
class UiInterface {
 public:
  enum Mode {
    STANDARD = 0,
    WEB_VR,
  };

  enum Direction {
    NONE = 0,
    LEFT,
    RIGHT,
    UP,
    DOWN,
  };

  explicit UiInterface(Mode initial_mode);
  virtual ~UiInterface() = default;

  // Set HTML UI state or pass events.
  void SetMode(Mode mode);
  void SetFullscreen(bool enabled);
  void SetSecurityLevel(int level);
  void SetWebVRSecureOrigin(bool secure);
  void SetLoading(bool loading);
  void SetLoadProgress(double progress);
  void InitTabList();
  void AppendToTabList(bool incognito, int id, const base::string16& title);
  void FlushTabList();
  void UpdateTab(bool incognito, int id, const std::string& title);
  void RemoveTab(bool incognito, int id);
  void SetURL(const GURL& url);
  void HandleAppButtonGesturePerformed(Direction direction);
  void HandleAppButtonClicked();
  void SetHistoryButtonsEnabled(bool can_go_back, bool can_go_forward);

 private:
  Mode mode_;
  bool fullscreen_ = false;

  DISALLOW_COPY_AND_ASSIGN(UiInterface);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_INTERFACE_H_
