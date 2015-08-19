// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/supervised_user_learn_more_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace options {

SupervisedUserLearnMoreHandler::SupervisedUserLearnMoreHandler() {
}

SupervisedUserLearnMoreHandler::~SupervisedUserLearnMoreHandler() {
}

void SupervisedUserLearnMoreHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "supervisedUserLearnMoreTitle",
        IDS_LEGACY_SUPERVISED_USER_LEARN_MORE_TITLE },
    { "supervisedUserLearnMoreDone",
        IDS_LEGACY_SUPERVISED_USER_LEARN_MORE_DONE_BUTTON },
  };

  base::string16 supervised_user_dashboard_display =
      base::ASCIIToUTF16(chrome::kLegacySupervisedUserManagementDisplayURL);
  localized_strings->SetString("supervisedUserLearnMoreText",
      l10n_util::GetStringFUTF16(IDS_LEGACY_SUPERVISED_USER_LEARN_MORE_TEXT,
                                 supervised_user_dashboard_display));

  RegisterStrings(localized_strings, resources, arraysize(resources));
}

}  // namespace options
