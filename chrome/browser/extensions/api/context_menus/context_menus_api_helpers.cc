// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/context_menus/context_menus_api_helpers.h"

#include "base/strings/string_number_conversions.h"

namespace extensions {
namespace context_menus_api_helpers {

const char kActionNotAllowedError[] =
    "Only extensions are allowed to use action contexts";
const char kCannotFindItemError[] = "Cannot find menu item with id *";
const char kCheckedError[] =
    "Only items with type \"radio\" or \"checkbox\" can be checked";
const char kDuplicateIDError[] =
    "Cannot create item with duplicate id *";
const char kGeneratedIdKey[] = "generatedId";
const char kLauncherNotAllowedError[] =
    "Only packaged apps are allowed to use 'launcher' context";
const char kOnclickDisallowedError[] = "Extensions using event pages cannot "
    "pass an onclick parameter to chrome.contextMenus.create. Instead, use "
    "the chrome.contextMenus.onClicked event.";
const char kParentsMustBeNormalError[] =
    "Parent items must have type \"normal\"";
const char kTitleNeededError[] =
    "All menu items except for separators must have a title";


std::string GetIDString(const MenuItem::Id& id) {
  if (id.uid == 0)
    return id.string_uid;
  else
    return base::IntToString(id.uid);
}

MenuItem* GetParent(MenuItem::Id parent_id,
                    const MenuManager* menu_manager,
                    std::string* error) {
  MenuItem* parent = menu_manager->GetItemById(parent_id);
  if (!parent) {
    *error = ErrorUtils::FormatErrorMessage(
        kCannotFindItemError, GetIDString(parent_id));
    return NULL;
  }
  if (parent->type() != MenuItem::NORMAL) {
    *error = kParentsMustBeNormalError;
    return NULL;
  }
  return parent;
}

}  // namespace context_menus_api_helpers
}  // namespace extensions
