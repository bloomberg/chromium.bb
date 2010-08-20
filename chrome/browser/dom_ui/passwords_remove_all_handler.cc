// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/passwords_remove_all_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/values.h"
#include "base/callback.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/dom_ui/passwords_exceptions_handler.h"

PasswordsRemoveAllHandler::PasswordsRemoveAllHandler() {
}

PasswordsRemoveAllHandler::~PasswordsRemoveAllHandler() {
}

void PasswordsRemoveAllHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString("remove_all_explanation",
      l10n_util::GetStringUTF16(
      IDS_PASSWORDS_PAGE_VIEW_TEXT_DELETE_ALL_PASSWORDS));
  localized_strings->SetString("remove_all_title",
      l10n_util::GetStringUTF16(
      IDS_PASSWORDS_PAGE_VIEW_CAPTION_DELETE_ALL_PASSWORDS));
  localized_strings->SetString("remove_all_yes",
      l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL));
  localized_strings->SetString("remove_all_no",
      l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL));
}

void PasswordsRemoveAllHandler::RegisterMessages() {
  DCHECK(dom_ui_);
}
