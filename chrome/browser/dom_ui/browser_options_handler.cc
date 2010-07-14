// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/browser_options_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/installer/util/browser_distribution.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

BrowserOptionsHandler::BrowserOptionsHandler() {
#if !defined(OS_MACOSX)
  default_browser_worker_ = new ShellIntegration::DefaultBrowserWorker(this);
#endif
}

BrowserOptionsHandler::~BrowserOptionsHandler() {
  if (default_browser_worker_.get())
    default_browser_worker_->ObserverDestroyed();
}

void BrowserOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString(L"startupGroupName",
      l10n_util::GetString(IDS_OPTIONS_STARTUP_GROUP_NAME));
  localized_strings->SetString(L"startupShowDefaultAndNewTab",
      l10n_util::GetString(IDS_OPTIONS_STARTUP_SHOW_DEFAULT_AND_NEWTAB));
  localized_strings->SetString(L"startupShowLastSession",
      l10n_util::GetString(IDS_OPTIONS_STARTUP_SHOW_LAST_SESSION));
  localized_strings->SetString(L"startupShowPages",
      l10n_util::GetString(IDS_OPTIONS_STARTUP_SHOW_PAGES));
  localized_strings->SetString(L"startupAddButton",
      l10n_util::GetString(IDS_OPTIONS_STARTUP_ADD_BUTTON));
  localized_strings->SetString(L"startupRemoveButton",
      l10n_util::GetString(IDS_OPTIONS_STARTUP_REMOVE_BUTTON));
  localized_strings->SetString(L"startupUseCurrent",
      l10n_util::GetString(IDS_OPTIONS_STARTUP_USE_CURRENT));
  localized_strings->SetString(L"homepageGroupName",
      l10n_util::GetString(IDS_OPTIONS_HOMEPAGE_GROUP_NAME));
  localized_strings->SetString(L"homepageUseNewTab",
      l10n_util::GetString(IDS_OPTIONS_HOMEPAGE_USE_NEWTAB));
  localized_strings->SetString(L"homepageUseURL",
      l10n_util::GetString(IDS_OPTIONS_HOMEPAGE_USE_URL));
  localized_strings->SetString(L"homepageShowButton",
      l10n_util::GetString(IDS_OPTIONS_HOMEPAGE_SHOW_BUTTON));
  localized_strings->SetString(L"defaultSearchGroupName",
      l10n_util::GetString(IDS_OPTIONS_DEFAULTSEARCH_GROUP_NAME));
  localized_strings->SetString(L"defaultSearchManageEnginesLink",
      l10n_util::GetString(IDS_OPTIONS_DEFAULTSEARCH_MANAGE_ENGINES_LINK));
  localized_strings->SetString(L"defaultBrowserGroupName",
      l10n_util::GetString(IDS_OPTIONS_DEFAULTBROWSER_GROUP_NAME));
  localized_strings->SetString(L"defaultBrowserUnknown",
      l10n_util::GetStringF(IDS_OPTIONS_DEFAULTBROWSER_UNKNOWN,
          l10n_util::GetString(IDS_PRODUCT_NAME)));
  localized_strings->SetString(L"defaultBrowserUseAsDefault",
      l10n_util::GetStringF(IDS_OPTIONS_DEFAULTBROWSER_USEASDEFAULT,
          l10n_util::GetString(IDS_PRODUCT_NAME)));
}

void BrowserOptionsHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback(
      "updateDefaultBrowserState",
      NewCallback(this, &BrowserOptionsHandler::UpdateDefaultBrowserState));
  dom_ui_->RegisterMessageCallback(
      "becomeDefaultBrowser",
      NewCallback(this, &BrowserOptionsHandler::BecomeDefaultBrowser));
}

void BrowserOptionsHandler::UpdateDefaultBrowserState(const Value* value) {
#if defined(OS_WIN)
  // Check for side-by-side first.
  if (!BrowserDistribution::GetDistribution()->CanSetAsDefault()) {
    SetDefaultBrowserUIString(IDS_OPTIONS_DEFAULTBROWSER_SXS);
    return;
  }
#endif

#if defined(OS_MACOSX)
  ShellIntegration::DefaultBrowserState state =
      ShellIntegration::IsDefaultBrowser();
  int status_string_id;
  if (state == ShellIntegration::IS_DEFAULT_BROWSER)
    status_string_id = IDS_OPTIONS_DEFAULTBROWSER_DEFAULT;
  else if (state == ShellIntegration::NOT_DEFAULT_BROWSER)
    status_string_id = IDS_OPTIONS_DEFAULTBROWSER_NOTDEFAULT;
  else
    status_string_id = IDS_OPTIONS_DEFAULTBROWSER_UNKNOWN;

  SetDefaultBrowserUIString(status_string_id);
#else
  default_browser_worker_->StartCheckDefaultBrowser();
#endif
}

void BrowserOptionsHandler::BecomeDefaultBrowser(const Value* value) {
#if defined(OS_MACOSX)
  if (ShellIntegration::SetAsDefaultBrowser())
    UpdateDefaultBrowserState(NULL);
#else
  default_browser_worker_->StartSetAsDefaultBrowser();
  // Callback takes care of updating UI.
#endif
}

int BrowserOptionsHandler::StatusStringIdForState(
    ShellIntegration::DefaultBrowserState state) {
  if (state == ShellIntegration::IS_DEFAULT_BROWSER)
    return IDS_OPTIONS_DEFAULTBROWSER_DEFAULT;
  if (state == ShellIntegration::NOT_DEFAULT_BROWSER)
    return IDS_OPTIONS_DEFAULTBROWSER_NOTDEFAULT;
  return IDS_OPTIONS_DEFAULTBROWSER_UNKNOWN;
}

void BrowserOptionsHandler::SetDefaultBrowserUIState(
    ShellIntegration::DefaultBrowserUIState state) {
  int status_string_id;
  if (state == ShellIntegration::STATE_IS_DEFAULT)
    status_string_id = IDS_OPTIONS_DEFAULTBROWSER_DEFAULT;
  else if (state == ShellIntegration::STATE_NOT_DEFAULT)
    status_string_id = IDS_OPTIONS_DEFAULTBROWSER_NOTDEFAULT;
  else if (state == ShellIntegration::STATE_UNKNOWN)
    status_string_id = IDS_OPTIONS_DEFAULTBROWSER_UNKNOWN;
  else
    return;  // Still processing.

  SetDefaultBrowserUIString(status_string_id);
}

void BrowserOptionsHandler::SetDefaultBrowserUIString(int status_string_id) {
  scoped_ptr<Value> status_string(Value::CreateStringValue(
      l10n_util::GetStringF(status_string_id,
                            l10n_util::GetString(IDS_PRODUCT_NAME))));

  scoped_ptr<Value> is_default(Value::CreateBooleanValue(
      status_string_id == IDS_OPTIONS_DEFAULTBROWSER_DEFAULT));

  dom_ui_->CallJavascriptFunction(
      L"BrowserOptions.updateDefaultBrowserStateCallback",
      *(status_string.get()), *(is_default.get()));
}

