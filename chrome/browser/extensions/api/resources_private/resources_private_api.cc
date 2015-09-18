// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/resources_private/resources_private_api.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/extensions/api/resources_private.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

// To add a new component to this API, simply:
// 1. Add your component to the Component enum in
//      chrome/common/extensions/api/resources_private.idl
// 2. Create an AddStringsForMyComponent(base::DictionaryValue * dict) method.
// 3. Tie in that method to the switch statement in Run()

namespace extensions {

namespace {

void AddStringsForIdentity(base::DictionaryValue* dict) {
  dict->SetString("window-title",
                  l10n_util::GetStringUTF16(IDS_EXTENSION_CONFIRM_PERMISSIONS));
}

void AddStringsForPdf(base::DictionaryValue* dict) {
  dict->SetString("passwordPrompt",
                  l10n_util::GetStringUTF16(IDS_PDF_NEED_PASSWORD));
  dict->SetString("pageLoading",
                  l10n_util::GetStringUTF16(IDS_PDF_PAGE_LOADING));
  dict->SetString("pageLoadFailed",
                  l10n_util::GetStringUTF16(IDS_PDF_PAGE_LOAD_FAILED));
}

}  // namespace

namespace get_strings = api::resources_private::GetStrings;

ResourcesPrivateGetStringsFunction::ResourcesPrivateGetStringsFunction() {}

ResourcesPrivateGetStringsFunction::~ResourcesPrivateGetStringsFunction() {}

ExtensionFunction::ResponseAction ResourcesPrivateGetStringsFunction::Run() {
  scoped_ptr<get_strings::Params> params(get_strings::Params::Create(*args_));
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);

  api::resources_private::Component component = params->component;

  switch (component) {
    case api::resources_private::COMPONENT_IDENTITY:
      AddStringsForIdentity(dict.get());
      break;
    case api::resources_private::COMPONENT_PDF:
      AddStringsForPdf(dict.get());
      break;
    case api::resources_private::COMPONENT_NONE:
      NOTREACHED();
  }

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, dict.get());

  return RespondNow(OneArgument(dict.Pass()));
}

}  // namespace extensions
