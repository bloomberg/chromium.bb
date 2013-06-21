// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/managed_user_learn_more_handler.h"

#include "base/values.h"
#include "grit/generated_resources.h"

namespace options {

ManagedUserLearnMoreHandler::ManagedUserLearnMoreHandler() {
}

ManagedUserLearnMoreHandler::~ManagedUserLearnMoreHandler() {
}

void ManagedUserLearnMoreHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "managedUserLearnMoreTitle", IDS_NEW_MANAGED_USER_LEARN_MORE_TITLE },
    { "managedUserLearnMoreText", IDS_NEW_MANAGED_USER_LEARN_MORE_TEXT },
    { "managedUserLearnMoreDone", IDS_NEW_MANAGED_USER_LEARN_MORE_DONE_BUTTON },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
}

}  // namespace options
