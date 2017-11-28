// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_AUTOCOMPLETE_CONTROLLER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_AUTOCOMPLETE_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_controller_delegate.h"
#include "url/gurl.h"

class AutocompleteController;
class ChromeAutocompleteProviderClient;
class Profile;

namespace vr {
class BrowserUiInterface;
}

namespace vr_shell {

class AutocompleteController : public AutocompleteControllerDelegate {
 public:
  explicit AutocompleteController(vr::BrowserUiInterface* ui);
  AutocompleteController();
  ~AutocompleteController() override;

  void Start(const base::string16& text);
  void Stop();

  // If |input| can be classified as URL, this function returns a GURL
  // representation of that URL. Otherwise, it returns a GURL which navigates
  // to the default search engine with |input| as query.
  // This function runs independently of any currently-running autocomplete
  // session.
  GURL GetUrlFromVoiceInput(const base::string16& input);

 private:
  void OnResultChanged(bool default_match_changed) override;

  Profile* profile_;
  ChromeAutocompleteProviderClient* client_;
  std::unique_ptr<::AutocompleteController> autocomplete_controller_;
  vr::BrowserUiInterface* ui_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteController);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_AUTOCOMPLETE_CONTROLLER_H_
