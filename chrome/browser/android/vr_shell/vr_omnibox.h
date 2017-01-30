// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_OMNIBOX_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_OMNIBOX_H_

#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_controller_delegate.h"

class AutocompleteController;
class Profile;

namespace vr_shell {

class UiInterface;

class VrOmnibox : public AutocompleteControllerDelegate {
 public:
  explicit VrOmnibox(UiInterface* ui);
  ~VrOmnibox() override;

  void HandleInput(const base::DictionaryValue& dict);

 private:
  void OnResultChanged(bool default_match_changed) override;

  UiInterface* ui_;

  Profile* profile_;
  std::unique_ptr<AutocompleteController> autocomplete_controller_;

  DISALLOW_COPY_AND_ASSIGN(VrOmnibox);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_OMNIBOX_H_
