// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/supervised_user_learn_more_handler.h"

#include "base/values.h"
#include "grit/generated_resources.h"

namespace options {

SupervisedUserLearnMoreHandler::SupervisedUserLearnMoreHandler() {
}

SupervisedUserLearnMoreHandler::~SupervisedUserLearnMoreHandler() {
}

void SupervisedUserLearnMoreHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "supervisedUserLearnMoreTitle", IDS_SUPERVISED_USER_LEARN_MORE_TITLE },
    { "supervisedUserLearnMoreText", IDS_SUPERVISED_USER_LEARN_MORE_TEXT },
    { "supervisedUserLearnMoreDone",
        IDS_SUPERVISED_USER_LEARN_MORE_DONE_BUTTON },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
}

}  // namespace options
