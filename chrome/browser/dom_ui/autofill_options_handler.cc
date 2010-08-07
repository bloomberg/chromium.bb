// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/autofill_options_handler.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/values.h"
#include "grit/generated_resources.h"

AutoFillOptionsHandler::AutoFillOptionsHandler() {
}

AutoFillOptionsHandler::~AutoFillOptionsHandler() {
}

void AutoFillOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("autoFillOptionsTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_OPTIONS_TITLE));
}

void AutoFillOptionsHandler::RegisterMessages() {
}
