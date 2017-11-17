// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_AUTOCOMPLETE_CONTROLLER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_AUTOCOMPLETE_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_controller_delegate.h"

class AutocompleteController;
class Profile;

namespace vr {

class BrowserUiInterface;

class AutocompleteController : public AutocompleteControllerDelegate {
 public:
  explicit AutocompleteController(BrowserUiInterface* ui);
  AutocompleteController();
  ~AutocompleteController() override;

  void Start(const base::string16& text);
  void Stop();

 private:
  void OnResultChanged(bool default_match_changed) override;

  Profile* profile_;
  std::unique_ptr<::AutocompleteController> autocomplete_controller_;
  BrowserUiInterface* ui_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteController);
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_AUTOCOMPLETE_CONTROLLER_H_
