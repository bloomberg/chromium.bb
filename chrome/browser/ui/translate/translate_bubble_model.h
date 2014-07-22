// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_MODEL_H_

#include <string>

#include "base/strings/string16.h"
#include "components/translate/core/common/translate_errors.h"

// The model for the Translate bubble UX. This manages the user's manipulation
// of the bubble and offers the data to show on the bubble.
class TranslateBubbleModel {
 public:
  enum ViewState {
    // The view state before translating.
    VIEW_STATE_BEFORE_TRANSLATE,

    // The view state during translating.
    VIEW_STATE_TRANSLATING,

    // The view state after translating.
    VIEW_STATE_AFTER_TRANSLATE,

    // The view state when an error of Translate happens.
    VIEW_STATE_ERROR,

    // The view state when the detailed settings is shown. This view appears
    // when the user click a link 'Advanced' on other views.
    VIEW_STATE_ADVANCED,
  };

  virtual ~TranslateBubbleModel() {}

  // Returns the current view state.
  virtual ViewState GetViewState() const = 0;

  // Transitions the view state.
  virtual void SetViewState(ViewState view_state) = 0;

  // Shows an error.
  virtual void ShowError(translate::TranslateErrors::Type error_type) = 0;

  // Goes back from the 'Advanced' view state.
  virtual void GoBackFromAdvanced() = 0;

  // Returns the number of languages supported.
  virtual int GetNumberOfLanguages() const = 0;

  // Returns the displayable name for the language at |index|.
  virtual base::string16 GetLanguageNameAt(int index) const = 0;

  // Returns the original language index.
  virtual int GetOriginalLanguageIndex() const = 0;

  // Updates the original language index.
  virtual void UpdateOriginalLanguageIndex(int index) = 0;

  // Returns the target language index.
  virtual int GetTargetLanguageIndex() const = 0;

  // Updates the target language index.
  virtual void UpdateTargetLanguageIndex(int index) = 0;

  // Sets the value if the user doesn't want to have the page translated in the
  // current page's language.
  virtual void SetNeverTranslateLanguage(bool value) = 0;

  // Sets the value if the user doesn't want to have the page translated the
  // current page's domain.
  virtual void SetNeverTranslateSite(bool value) = 0;

  // Returns true if the webpage in the current original language should be
  // translated into the current target language automatically.
  virtual bool ShouldAlwaysTranslate() const = 0;

  // Sets the value if the webpage in the current original language should be
  // translated into the current target language automatically.
  virtual void SetAlwaysTranslate(bool value) = 0;

  // Starts translating the current page.
  virtual void Translate() = 0;

  // Reverts translation.
  virtual void RevertTranslation() = 0;

  // Processes when the user dismiss the bubble without translating.
  // |explicitly_closed| is true if the user explicitly closed the UI, for
  // example, clicking 'Nope' button.
  virtual void TranslationDeclined(bool explicitly_closed) = 0;

  // Returns true if the page is translated in the currently selected source
  // and target language.
  virtual bool IsPageTranslatedInCurrentLanguages() const = 0;
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_MODEL_H_
