// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/identity_private/identity_private_api.h"

#include <string>

#include "base/values.h"
#include "grit/generated_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace extensions {

IdentityPrivateGetResourcesFunction::IdentityPrivateGetResourcesFunction() {}

IdentityPrivateGetResourcesFunction::~IdentityPrivateGetResourcesFunction() {}

bool IdentityPrivateGetResourcesFunction::RunImpl() {
  base::DictionaryValue* result = new base::DictionaryValue;

  result->SetString("IDR_CLOSE_DIALOG",
                    webui::GetBitmapDataUrlFromResource(IDR_CLOSE_DIALOG));
  result->SetString("IDR_CLOSE_DIALOG_H",
                    webui::GetBitmapDataUrlFromResource(IDR_CLOSE_DIALOG_H));
  result->SetString("IDR_CLOSE_DIALOG_P",
                    webui::GetBitmapDataUrlFromResource(IDR_CLOSE_DIALOG_P));
  result->SetString(
      "IDS_EXTENSION_PERMISSIONS_PROMPT_TITLE",
      l10n_util::GetStringUTF16(IDS_EXTENSION_PERMISSIONS_PROMPT_TITLE));

  SetResult(result);
  return true;
}

}  // extensions
