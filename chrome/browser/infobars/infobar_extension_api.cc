// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_extension_api.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"

bool InfobarsShowFunction::RunSync() {
  base::DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  const char kTabId[] = "tabId";
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(kTabId, &tab_id));

  const char kHtmlPath[] = "path";
  std::string html_path;
  EXTENSION_FUNCTION_VALIDATE(args->GetString(kHtmlPath, &html_path));

  const char kHeight[] = "height";
  int height = 0;
  if (args->HasKey(kHeight))
    EXTENSION_FUNCTION_VALIDATE(args->GetInteger(kHeight, &height));

  Browser* browser = NULL;
  content::WebContents* web_contents = NULL;
  if (!extensions::ExtensionTabUtil::GetTabById(tab_id, GetProfile(),
                                                include_incognito(), &browser,
                                                NULL, &web_contents, NULL)) {
    error_ = extensions::ErrorUtils::FormatErrorMessage(
        extensions::tabs_constants::kTabNotFoundError,
        base::IntToString(tab_id));
    return false;
  }
  GURL url(extension()->GetResourceURL(extension()->url(), html_path));
  ExtensionInfoBarDelegate::Create(
      web_contents, browser, extension(), url, height);

  // TODO(finnur): Return the actual DOMWindow object. Bug 26463.
  DCHECK(browser->extension_window_controller());
  SetResult(browser->extension_window_controller()->CreateWindowValue());

  return true;
}
