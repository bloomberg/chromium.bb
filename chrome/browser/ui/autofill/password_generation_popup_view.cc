// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>

#include "chrome/browser/ui/autofill/password_generation_popup_view.h"

// Non-aura is not implemented yet.
#if !defined(USE_AURA)

namespace autofill {

PasswordGenerationPopupView* PasswordGenerationPopupView::Create(
    PasswordGenerationPopupController* controller) {
  return NULL;
}

}  // namespace autofill

#endif  // #if !defined(USE_AURA)
