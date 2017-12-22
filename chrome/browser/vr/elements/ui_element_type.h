// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_TYPE_H_
#define CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_TYPE_H_

#include <string>

namespace vr {

// These identifiers serve as stable, semantic identifiers for UI elements.
// These are not unique, analogous to CSS classes.
enum UiElementType {
  kTypeNone = 0,
  kTypeButtonBackground,
  kTypeButtonForeground,
  kTypeButtonHitTarget,
  kTypeScaledDepthAdjuster,
  kTypeOmniboxSuggestionBackground,
  kTypeOmniboxSuggestionLayout,
  kTypeOmniboxSuggestionTextLayout,
  kTypeOmniboxSuggestionIconField,
  kTypeOmniboxSuggestionIcon,
  kTypeOmniboxSuggestionContentText,
  kTypeOmniboxSuggestionDescriptionText,
  kTypeOmniboxSuggestionSpacer,
  kTypeTextInputHint,
  kTypeTextInputText,
  kTypeTextInputCursor,
  kTypeToastBackground,
  kTypeToastContainer,
  kTypeToastIcon,
  kTypeToastText,

  // This must be last.
  kNumUiElementTypes,
};

std::string UiElementTypeToString(UiElementType type);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_TYPE_H_
