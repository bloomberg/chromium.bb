// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_page_actions_api.h"

#include <string>

#include "base/string_number_conversions.h"
#include "chrome/browser/extensions/api/extension_action/extension_page_actions_api_constants.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/error_utils.h"

using content::NavigationEntry;
using extensions::ErrorUtils;

namespace keys = extension_page_actions_api_constants;

namespace {
// Errors.
const char kNoTabError[] = "No tab with id: *.";
const char kNoPageActionError[] =
    "This extension has no page action specified.";
const char kUrlNotActiveError[] = "This url is no longer active: *.";
}

PageActionsFunction::PageActionsFunction() {
}

PageActionsFunction::~PageActionsFunction() {
}

bool PageActionsFunction::SetPageActionEnabled(bool enable) {
  std::string extension_action_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &extension_action_id));
  DictionaryValue* action = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &action));

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(action->GetInteger(keys::kTabIdKey, &tab_id));
  std::string url;
  EXTENSION_FUNCTION_VALIDATE(action->GetString(keys::kUrlKey, &url));

  std::string title;
  if (enable) {
    if (action->HasKey(keys::kTitleKey))
      EXTENSION_FUNCTION_VALIDATE(action->GetString(keys::kTitleKey, &title));
  }

  ExtensionAction* page_action =
      extensions::ExtensionActionManager::Get(profile())->
      GetPageAction(*GetExtension());
  if (!page_action) {
    error_ = kNoPageActionError;
    return false;
  }

  // Find the WebContents that contains this tab id.
  content::WebContents* contents = NULL;
  bool result = ExtensionTabUtil::GetTabById(
      tab_id, profile(), include_incognito(), NULL, NULL, &contents, NULL);
  if (!result || !contents) {
    error_ = ErrorUtils::FormatErrorMessage(
        kNoTabError, base::IntToString(tab_id));
    return false;
  }

  // Make sure the URL hasn't changed.
  NavigationEntry* entry = contents->GetController().GetActiveEntry();
  if (!entry || url != entry->GetURL().spec()) {
    error_ = ErrorUtils::FormatErrorMessage(kUrlNotActiveError, url);
    return false;
  }

  // Set visibility and broadcast notifications that the UI should be updated.
  page_action->SetAppearance(
      tab_id, enable ? ExtensionAction::ACTIVE : ExtensionAction::INVISIBLE);
  page_action->SetTitle(tab_id, title);
  extensions::TabHelper::FromWebContents(contents)->
      location_bar_controller()->NotifyChange();

  return true;
}

bool EnablePageActionsFunction::RunImpl() {
  return SetPageActionEnabled(true);
}

bool DisablePageActionsFunction::RunImpl() {
  return SetPageActionEnabled(false);
}
