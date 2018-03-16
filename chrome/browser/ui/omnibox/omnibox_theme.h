// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_THEME_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_THEME_H_

#include "third_party/skia/include/core/SkColor.h"

// A part of the omnibox (location bar, location bar decoration, or dropdown).
enum class OmniboxPart {
  RESULTS_BACKGROUND,  // Background of the results dropdown.
  RESULTS_SEPARATOR,   // Separator between the input row and the results rows.

  // Text styles.
  TEXT_DEFAULT,
  TEXT_DIMMED,
  TEXT_INVISIBLE,
  TEXT_NEGATIVE,
  TEXT_POSITIVE,
  TEXT_URL,
};

// The tint of the omnibox theme. E.g. Incognito may use a DARK tint. NATIVE is
// only used on Desktop Linux.
enum class OmniboxTint { DARK, LIGHT, NATIVE };

// An optional state for a given |OmniboxPart|.
enum class OmniboxPartState { NORMAL, HOVERED, SELECTED, HOVERED_AND_SELECTED };

// Returns the color for the given |part| and |tint|. An optional |state| can be
// provided for OmniboxParts that support stateful colors.
SkColor GetOmniboxColor(OmniboxPart part,
                        OmniboxTint tint,
                        OmniboxPartState state = OmniboxPartState::NORMAL);

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_THEME_H_
