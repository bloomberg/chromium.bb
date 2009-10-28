// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_manager.h"

#include "base/command_line.h"
#include "chrome/browser/autofill/autofill_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "webkit/glue/form_field_values.h"

AutoFillManager::AutoFillManager(TabContents* tab_contents)
    : tab_contents_(tab_contents),
      infobar_(NULL) {
}

AutoFillManager::~AutoFillManager() {
}

void AutoFillManager::FormFieldValuesSubmitted(
    const webkit_glue::FormFieldValues& form) {
  // TODO(jhawkins): Remove this switch when AutoFill++ is fully implemented.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNewAutoFill))
    return;

  // Grab a copy of the form data.
  form_data_.reset(new webkit_glue::FormFieldValues(form));

  // Ask the user for permission to save form information.
  infobar_.reset(new AutoFillInfoBarDelegate(tab_contents_, this));
}

void AutoFillManager::SaveFormData() {
  // TODO(jhawkins): Save the form data to the web database.
}

void AutoFillManager::Reset() {
  form_data_.reset();
}
