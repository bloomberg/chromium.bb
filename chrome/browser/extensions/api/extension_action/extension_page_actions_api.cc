// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_page_actions_api.h"

#include <string>

#include "base/string_number_conversions.h"
#include "chrome/browser/extensions/api/extension_action/extension_page_actions_api_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
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

bool PageActionFunction::RunImpl() {
  ExtensionActionFunction::RunImpl();

  // If the first argument is an integer, it is the tabId.
  base::Value* arg = NULL;
  args_->Get(0, &arg);
  if (arg->IsType(base::Value::TYPE_INTEGER))
    arg->GetAsInteger(&tab_id_);

  extension_action_ = GetExtension()->page_action();
  if (!extension_action_) {
    error_ = kNoPageActionError;
    return false;
  }

  // Find the TabContentsWrapper that contains this tab id.
  contents_ = NULL;
  TabContentsWrapper* wrapper = NULL;
  bool result = ExtensionTabUtil::GetTabById(
      tab_id_, profile(), include_incognito(), NULL, NULL, &wrapper, NULL);
  if (!result || !wrapper) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kNoTabError, base::IntToString(tab_id_));
    return false;
  }
  contents_ = wrapper;

  if (!RunExtensionAction())
    return false;

  return true;
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

  // Find the TabContentsWrapper that contains this tab id.
  TabContentsWrapper* contents = NULL;
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
  page_action->SetIsVisible(tab_id, enable);
  page_action->SetTitle(tab_id, title);
  page_action->SetIconIndex(tab_id, icon_id);
  contents->extension_tab_helper()->PageActionStateChanged();

  return true;
}

bool PageActionFunction::SetVisible(bool visible) {
  extension_action_->SetIsVisible(tab_id_, visible);
  contents_->extension_tab_helper()->PageActionStateChanged();
  return true;
}

bool EnablePageActionsFunction::RunImpl() {
  return SetPageActionEnabled(true);
}

bool DisablePageActionsFunction::RunImpl() {
  return SetPageActionEnabled(false);
}

bool PageActionShowFunction::RunExtensionAction() {
  return SetVisible(true);
}

bool PageActionHideFunction::RunExtensionAction() {
  return SetVisible(false);
}

bool PageActionSetIconFunction::RunExtensionAction() {
  // setIcon can take a variant argument: either a canvas ImageData, or an
  // icon index.
  base::BinaryValue* binary = NULL;
  int icon_index;
  if (details_->GetBinary("imageData", &binary)) {
    SetIcon();
  } else if (details_->GetInteger("iconIndex", &icon_index)) {
    if (icon_index < 0 || static_cast<size_t>(icon_index) >=
                              extension_action_->icon_paths()->size()) {
      error_ = kIconIndexOutOfBounds;
      return false;
    }
    extension_action_->SetIcon(tab_id_, SkBitmap());
    extension_action_->SetIconIndex(tab_id_, icon_index);
  } else {
    EXTENSION_FUNCTION_VALIDATE(false);
  }
  contents_->extension_tab_helper()->PageActionStateChanged();
  return true;
}

bool PageActionSetTitleFunction::RunExtensionAction() {
  if (!SetTitle())
    return false;
  contents_->extension_tab_helper()->PageActionStateChanged();
  return true;
}

bool PageActionSetPopupFunction::RunExtensionAction() {
  // TODO(skerner): Consider allowing null and undefined to mean the popup
  // should be removed.
  if (!SetPopup())
    return false;
  contents_->extension_tab_helper()->PageActionStateChanged();
  return true;
}

// Not currently exposed to extensions. To re-enable, add mapping in
// extension_function_dispatcher.
bool PageActionSetBadgeBackgroundColorFunction::RunExtensionAction() {
  if (!SetBadgeBackgroundColor())
    return false;
  contents_->extension_tab_helper()->PageActionStateChanged();
  return true;
}

// Not currently exposed to extensions. To re-enable, add mapping in
// extension_function_dispatcher.
bool PageActionSetBadgeTextColorFunction::RunExtensionAction() {
  ListValue* color_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(details_->GetList("color", &color_value));
  EXTENSION_FUNCTION_VALIDATE(color_value->GetSize() == 4);

  int color_array[4] = {0};
  for (size_t i = 0; i < arraysize(color_array); ++i)
    EXTENSION_FUNCTION_VALIDATE(color_value->GetInteger(i, &color_array[i]));

  SkColor color = SkColorSetARGB(color_array[3], color_array[0], color_array[1],
                                 color_array[2]);
  extension_action_->SetBadgeTextColor(tab_id_, color);
  contents_->extension_tab_helper()->PageActionStateChanged();
  return true;
}

// Not currently exposed to extensions. To re-enable, add mapping in
// extension_function_dispatcher.
bool PageActionSetBadgeTextFunction::RunExtensionAction() {
  if (!SetBadgeText())
    return false;
  contents_->extension_tab_helper()->PageActionStateChanged();
  return true;
}

bool PageActionGetTitleFunction::RunExtensionAction() {
  return GetTitle();
}

bool PageActionGetPopupFunction::RunExtensionAction() {
  return GetPopup();
}

// Not currently exposed to extensions. To re-enable, add mapping in
// extension_function_dispatcher.
bool PageActionGetBadgeBackgroundColorFunction::RunExtensionAction() {
  return GetBadgeBackgroundColor();
}

// Not currently exposed to extensions. To re-enable, add mapping in
// extension_function_dispatcher.
bool PageActionGetBadgeTextFunction::RunExtensionAction() {
  return GetBadgeText();
}
