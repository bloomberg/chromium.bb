// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_infobar_module.h"

#include "base/string_util.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_infobar_module_constants.h"
#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_tabs_module_constants.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"

namespace keys = extension_infobar_module_constants;

bool ShowInfoBarFunction::RunImpl() {
  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(keys::kTabId, &tab_id));

  std::string html_path;
  EXTENSION_FUNCTION_VALIDATE(args->GetString(keys::kHtmlPath, &html_path));

  const Extension* extension = GetExtension();
  GURL url = extension->GetResourceURL(extension->url(), html_path);

  Browser* browser = NULL;
  TabContentsWrapper* tab_contents = NULL;
  if (!ExtensionTabUtil::GetTabById(
      tab_id,
      profile(),
      include_incognito(),
      &browser,
      NULL,
      &tab_contents,
      NULL)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        extension_tabs_module_constants::kTabNotFoundError,
        base::IntToString(tab_id));
    return false;
  }

  tab_contents->tab_contents()->AddInfoBar(
      new ExtensionInfoBarDelegate(browser, tab_contents->tab_contents(),
                                   GetExtension(), url));

  // TODO(finnur): Return the actual DOMWindow object. Bug 26463.
  result_.reset(ExtensionTabUtil::CreateWindowValue(browser, false));

  return true;
}
