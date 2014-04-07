// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_DRIVER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_DRIVER_H_

#include <string>

class LanguageState;

// Interface that allows Translate core code to interact with its driver (i.e.,
// obtain information from it and give information to it). A concrete
// implementation must be provided by the driver.
class TranslateDriver {
 public:
  // Returns true if the current page was navigated through a link.
  virtual bool IsLinkNavigation() = 0;

  // Called when Translate is enabled or disabled.
  virtual void OnTranslateEnabledChanged() = 0;

  // Called when the page is "translated" state of the page changed.
  virtual void OnIsPageTranslatedChanged() = 0;

  // Gets the LanguageState associated with the driver.
  virtual LanguageState& GetLanguageState() = 0;

  // Translates the page contents from |source_lang| to |target_lang|.
  virtual void TranslatePage(const std::string& translate_script,
                             const std::string& source_lang,
                             const std::string& target_lang) = 0;

  // Reverts the contents of the page to its original language.
  virtual void RevertTranslation() = 0;
};

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_DRIVER_H_
