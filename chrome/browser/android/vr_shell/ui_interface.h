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

class VrOmnibox;

class UiCommandHandler {
 public:
  virtual void SendCommandToUi(const base::Value& value) = 0;
};

// This class manages the communication of browser state from VR shell to the
// HTML UI. State information is asynchronous and unidirectional.
class UiInterface {
 public:
  enum Mode {
    STANDARD = 0,
    WEB_VR,
  };

  explicit UiInterface(Mode initial_mode);
  virtual ~UiInterface();

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
  void SetOmniboxSuggestions(std::unique_ptr<base::Value> suggestions);
  void HandleAppButtonClicked();

  // Handlers for HTML UI commands and notifications.
  void OnDomContentsLoaded();
  void HandleOmniboxInput(const base::DictionaryValue& input);

  void SetUiCommandHandler(UiCommandHandler* handler);

 private:
  void FlushUpdates();
  void FlushModeState();

  Mode mode_;
  bool fullscreen_ = false;
  UiCommandHandler* handler_;
  bool loaded_ = false;
  base::DictionaryValue updates_;
  std::unique_ptr<base::ListValue> tab_list_;

  std::unique_ptr<VrOmnibox> omnibox_;

  DISALLOW_COPY_AND_ASSIGN(UiInterface);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_INTERFACE_H_
