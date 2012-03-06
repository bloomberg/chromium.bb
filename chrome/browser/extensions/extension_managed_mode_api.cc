// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of the Chrome Extensions Managed Mode API.

#include "chrome/browser/extensions/extension_managed_mode_api.h"

#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_preference_api_constants.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

namespace {

// Key to report whether the attempt to enter managed mode succeeded.
const char kEnterSuccessKey[] = "success";

}  // namespace

namespace keys = extension_preference_api_constants;

GetManagedModeFunction::~GetManagedModeFunction() { }

bool GetManagedModeFunction::RunImpl() {
  PrefService* local_state = g_browser_process->local_state();
  bool in_managed_mode = local_state->GetBoolean(prefs::kInManagedMode);

  scoped_ptr<DictionaryValue> result(new DictionaryValue);
  result->SetBoolean(keys::kValue, in_managed_mode);
  result_.reset(result.release());
  return true;
}

EnterManagedModeFunction::~EnterManagedModeFunction() { }

bool EnterManagedModeFunction::RunImpl() {
  PrefService* local_state = g_browser_process->local_state();
  bool in_managed_mode = local_state->GetBoolean(prefs::kInManagedMode);

  bool confirmed = true;
  if (!in_managed_mode) {
    // TODO(pamg): WIP. Show modal password dialog and save hashed password. Set
    //     |confirmed| to false if user cancels dialog.

    if (confirmed)
      local_state->SetBoolean(prefs::kInManagedMode, true);
  }

  scoped_ptr<DictionaryValue> result(new DictionaryValue);
  result->SetBoolean(kEnterSuccessKey, confirmed);
  result_.reset(result.release());
  return true;
}
