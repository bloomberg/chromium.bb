// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity_private/identity_private_api.h"

#include <string>

#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

IdentityPrivateGetStringsFunction::IdentityPrivateGetStringsFunction() {}

IdentityPrivateGetStringsFunction::~IdentityPrivateGetStringsFunction() {}

bool IdentityPrivateGetStringsFunction::RunSync() {
  base::DictionaryValue* dict = new base::DictionaryValue;
  SetResult(dict);

  dict->SetString(
      "window-title",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PERMISSIONS_PROMPT_TITLE));

  return true;
}

}  // namespace extensions
