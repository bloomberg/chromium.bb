// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/passwords_exceptions_handler.h"

#include "app/l10n_util.h"
#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/profile.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

PasswordsExceptionsHandler::PasswordsExceptionsHandler() {
}

PasswordsExceptionsHandler::~PasswordsExceptionsHandler() {
}

void PasswordsExceptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString(L"passwordsExceptionsTitle",
      l10n_util::GetString(IDS_PASSWORDS_EXCEPTIONS_WINDOW_TITLE));
  localized_strings->SetString(L"passwordsTabTitle",
      l10n_util::GetString(IDS_PASSWORDS_SHOW_PASSWORDS_TAB_TITLE));
  localized_strings->SetString(L"exceptionsTabTitle",
      l10n_util::GetString(IDS_PASSWORDS_EXCEPTIONS_TAB_TITLE));
}

void PasswordsExceptionsHandler::RegisterMessages() {
}
