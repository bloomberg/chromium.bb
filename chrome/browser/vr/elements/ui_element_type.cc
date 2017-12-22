// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/ui_element_type.h"

#include "base/logging.h"
#include "base/macros.h"

namespace vr {

namespace {

static const char* g_ui_element_type_strings[] = {
    "kTypeNone",
    "kTypeButtonBackground",
    "kTypeButtonForeground",
    "kTypeButtonHitTarget",
    "kTypeScaledDepthAdjuster",
    "kTypeOmniboxSuggestionBackground",
    "kTypeOmniboxSuggestionLayout",
    "kTypeOmniboxSuggestionTextLayout",
    "kTypeOmniboxSuggestionIconField",
    "kTypeOmniboxSuggestionIcon",
    "kTypeOmniboxSuggestionContentText",
    "kTypeOmniboxSuggestionDescriptionText",
    "kTypeOmniboxSuggestionSpacer",
    "kTypeTextInputHint",
    "kTypeTextInputText",
    "kTypeTextInputCursor",
    "kTypeToastBackground",
    "kTypeToastContainer",
    "kTypeToastIcon",
    "kTypeToastText",
};

static_assert(
    kNumUiElementTypes == arraysize(g_ui_element_type_strings),
    "Mismatch between the kUiElementName enum and the corresponding array "
    "of strings.");

}  // namespace

std::string UiElementTypeToString(UiElementType type) {
  DCHECK_GT(kNumUiElementTypes, type);
  return g_ui_element_type_strings[type];
}

}  // namespace vr
