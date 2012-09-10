// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_page_actions_api.h"

#include <string>

#include "base/string_number_conversions.h"
#include "chrome/browser/extensions/api/extension_action/extension_page_actions_api_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::NavigationEntry;

namespace keys = extension_page_actions_api_constants;

namespace {
// Errors.
const char kNoTabError[] = "No tab with id: *.";
const char kNoPageActionError[] =
    "This extension has no page action specified.";
const char kUrlNotActiveError[] = "This url is no longer active: *.";
const char kIconIndexOutOfBounds[] = "Page action icon index out of bounds.";
const char kNoIconSpecified[] = "Page action has no icons to show.";
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
  int icon_id = 0;
  if (enable) {
    // Both of those are optional.
    if (action->HasKey(keys::kTitleKey))
      EXTENSION_FUNCTION_VALIDATE(action->GetString(keys::kTitleKey, &title));
    if (action->HasKey(keys::kIconIdKey)) {
      EXTENSION_FUNCTION_VALIDATE(action->GetInteger(keys::kIconIdKey,
                                                     &icon_id));
    }
  }

  ExtensionAction* page_action = GetExtension()->page_action();
  if (!page_action) {
    error_ = kNoPageActionError;
    return false;
  }

  if (icon_id < 0 ||
      static_cast<size_t>(icon_id) >= page_action->icon_paths()->size()) {
    error_ = (icon_id == 0) ? kNoIconSpecified : kIconIndexOutOfBounds;
    return false;
  }

  // Find the TabContents that contains this tab id.
  TabContents* contents = NULL;
  bool result = ExtensionTabUtil::GetTabById(
      tab_id, profile(), include_incognito(), NULL, NULL, &contents, NULL);
  if (!result || !contents) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kNoTabError, base::IntToString(tab_id));
    return false;
  }

  // Make sure the URL hasn't changed.
  NavigationEntry* entry =
      contents->web_contents()->GetController().GetActiveEntry();
  if (!entry || url != entry->GetURL().spec()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(kUrlNotActiveError, url);
    return false;
  }

  // Set visibility and broadcast notifications that the UI should be updated.
  page_action->SetAppearance(
      tab_id, enable ? ExtensionAction::ACTIVE : ExtensionAction::INVISIBLE);
  page_action->SetTitle(tab_id, title);
  page_action->SetIconIndex(tab_id, icon_id);
  extensions::TabHelper::FromWebContents(contents->web_contents())->
      location_bar_controller()->NotifyChange();

  return true;
}

bool EnablePageActionsFunction::RunImpl() {
  return SetPageActionEnabled(true);
}

bool DisablePageActionsFunction::RunImpl() {
  return SetPageActionEnabled(false);
}
