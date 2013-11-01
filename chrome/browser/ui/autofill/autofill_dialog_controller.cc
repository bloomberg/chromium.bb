// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"

namespace autofill {

AutofillDialogController::~AutofillDialogController() {}

#if !defined(ENABLE_AUTOFILL_DIALOG)
// static
base::WeakPtr<AutofillDialogController>
AutofillDialogController::Create(
    content::WebContents* contents,
    const FormData& form_structure,
    const GURL& source_url,
    const base::Callback<void(const FormStructure*)>& callback) {
  NOTIMPLEMENTED();
  return base::WeakPtr<AutofillDialogController>();
}

// static
void AutofillDialogController::RegisterPrefs(PrefRegistrySimple* registry) {}

// static
void AutofillDialogController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {}
#endif  //  !defined(ENABLE_AUTOFILL_DIALOG)

}  // namespace autofill
