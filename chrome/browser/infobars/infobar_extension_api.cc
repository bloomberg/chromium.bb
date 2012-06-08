// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_extension_api.h"

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_window_controller.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"

using extensions::Extension;

namespace {

const char kHtmlPath[] = "path";
const char kTabId[] = "tabId";
const char kHeight[] = "height";

const char kNoCurrentWindowError[] = "No current browser window was found";
const char kTabNotFoundError[] = "Specified tab (or default tab) not found";

}  // namespace

bool ShowInfoBarFunction::RunImpl() {
  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(kTabId, &tab_id));

  std::string html_path;
  EXTENSION_FUNCTION_VALIDATE(args->GetString(kHtmlPath, &html_path));

  int height = 0;
  if (args->HasKey(kHeight))
    EXTENSION_FUNCTION_VALIDATE(args->GetInteger(kHeight, &height));

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
        extensions::tabs_constants::kTabNotFoundError,
        base::IntToString(tab_id));
    return false;
  }

  tab_contents->infobar_tab_helper()->AddInfoBar(
      new ExtensionInfoBarDelegate(browser, tab_contents->infobar_tab_helper(),
                                   GetExtension(), url, height));

  // TODO(finnur): Return the actual DOMWindow object. Bug 26463.
  DCHECK(browser->extension_window_controller());
  result_.reset(browser->extension_window_controller()->CreateWindowValue());

  return true;
}
