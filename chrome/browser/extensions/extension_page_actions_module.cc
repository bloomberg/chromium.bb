// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_page_actions_module.h"

#include <string>

#include "base/string_number_conversions.h"
#include "chrome/browser/extensions/extension_page_actions_module_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/render_messages.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace keys = extension_page_actions_module_constants;

namespace {
// Errors.
const char kNoTabError[] = "No tab with id: *.";
const char kNoPageActionError[] =
    "This extension has no page action specified.";
const char kUrlNotActiveError[] = "This url is no longer active: *.";
const char kIconIndexOutOfBounds[] = "Page action icon index out of bounds.";
const char kNoIconSpecified[] = "Page action has no icons to show.";
}

// TODO(EXTENSIONS_DEPRECATED): obsolete API.
bool PageActionFunction::SetPageActionEnabled(bool enable) {
  std::string page_action_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &page_action_id));
  DictionaryValue* action;
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
  TabContentsWrapper* contents = NULL;
  bool result = ExtensionTabUtil::GetTabById(
      tab_id, profile(), include_incognito(), NULL, NULL, &contents, NULL);
  if (!result || !contents) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kNoTabError, base::IntToString(tab_id));
    return false;
  }

  // Make sure the URL hasn't changed.
  NavigationEntry* entry = contents->controller().GetActiveEntry();
  if (!entry || url != entry->url().spec()) {
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

bool PageActionFunction::InitCommon(int tab_id) {
  page_action_ = GetExtension()->page_action();
  if (!page_action_) {
    error_ = kNoPageActionError;
    return false;
  }

  // Find the TabContents that contains this tab id.
  contents_ = NULL;
  TabContentsWrapper* wrapper = NULL;
  bool result = ExtensionTabUtil::GetTabById(
      tab_id, profile(), include_incognito(), NULL, NULL, &wrapper, NULL);
  if (!result || !wrapper) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kNoTabError, base::IntToString(tab_id));
    return false;
  }
  contents_ = wrapper;

  return true;
}

bool PageActionFunction::SetVisible(bool visible) {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &tab_id));
  if (!InitCommon(tab_id))
    return false;

  page_action_->SetIsVisible(tab_id, visible);
  contents_->extension_tab_helper()->PageActionStateChanged();
  return true;
}

bool EnablePageActionFunction::RunImpl() {
  return SetPageActionEnabled(true);
}

bool DisablePageActionFunction::RunImpl() {
  return SetPageActionEnabled(false);
}

bool PageActionShowFunction::RunImpl() {
  return SetVisible(true);
}

bool PageActionHideFunction::RunImpl() {
  return SetVisible(false);
}

bool PageActionSetIconFunction::RunImpl() {
  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger("tabId", &tab_id));
  if (!InitCommon(tab_id))
    return false;

  // setIcon can take a variant argument: either a canvas ImageData, or an
  // icon index.
  BinaryValue* binary;
  int icon_index;
  if (args->GetBinary("imageData", &binary)) {
    IPC::Message bitmap_pickle(binary->GetBuffer(), binary->GetSize());
    void* iter = NULL;
    scoped_ptr<SkBitmap> bitmap(new SkBitmap);
    EXTENSION_FUNCTION_VALIDATE(
        IPC::ReadParam(&bitmap_pickle, &iter, bitmap.get()));
    page_action_->SetIcon(tab_id, *bitmap);
  } else if (args->GetInteger("iconIndex", &icon_index)) {
    if (icon_index < 0 || static_cast<size_t>(icon_index) >=
                              page_action_->icon_paths()->size()) {
      error_ = kIconIndexOutOfBounds;
      return false;
    }
    page_action_->SetIcon(tab_id, SkBitmap());
    page_action_->SetIconIndex(tab_id, icon_index);
  } else {
    EXTENSION_FUNCTION_VALIDATE(false);
  }

  contents_->extension_tab_helper()->PageActionStateChanged();
  return true;
}

bool PageActionSetTitleFunction::RunImpl() {
  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger("tabId", &tab_id));
  if (!InitCommon(tab_id))
    return false;

  std::string title;
  EXTENSION_FUNCTION_VALIDATE(args->GetString("title", &title));

  page_action_->SetTitle(tab_id, title);
  contents_->extension_tab_helper()->PageActionStateChanged();
  return true;
}

bool PageActionSetPopupFunction::RunImpl() {
  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger("tabId", &tab_id));
  if (!InitCommon(tab_id))
    return false;

  // TODO(skerner): Consider allowing null and undefined to mean the popup
  // should be removed.
  std::string popup_string;
  EXTENSION_FUNCTION_VALIDATE(args->GetString("popup", &popup_string));

  GURL popup_url;
  if (!popup_string.empty())
    popup_url = GetExtension()->GetResourceURL(popup_string);

  page_action_->SetPopupUrl(tab_id, popup_url);
  contents_->extension_tab_helper()->PageActionStateChanged();
  return true;
}

// Not currently exposed to extensions. To re-enable, add mapping in
// extension_function_dispatcher.
bool PageActionSetBadgeBackgroundColorFunction::RunImpl() {
  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger("tabId", &tab_id));
  if (!InitCommon(tab_id))
    return false;

  ListValue* color_value;
  EXTENSION_FUNCTION_VALIDATE(args->GetList("color", &color_value));
  EXTENSION_FUNCTION_VALIDATE(color_value->GetSize() == 4);

  int color_array[4] = {0};
  for (size_t i = 0; i < arraysize(color_array); ++i)
    EXTENSION_FUNCTION_VALIDATE(color_value->GetInteger(i, &color_array[i]));

  SkColor color = SkColorSetARGB(color_array[3], color_array[0], color_array[1],
                                 color_array[2]);
  page_action_->SetBadgeBackgroundColor(tab_id, color);
  contents_->extension_tab_helper()->PageActionStateChanged();
  return true;
}

// Not currently exposed to extensions. To re-enable, add mapping in
// extension_function_dispatcher.
bool PageActionSetBadgeTextColorFunction::RunImpl() {
  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger("tabId", &tab_id));
  if (!InitCommon(tab_id))
    return false;

  ListValue* color_value;
  EXTENSION_FUNCTION_VALIDATE(args->GetList("color", &color_value));
  EXTENSION_FUNCTION_VALIDATE(color_value->GetSize() == 4);

  int color_array[4] = {0};
  for (size_t i = 0; i < arraysize(color_array); ++i)
    EXTENSION_FUNCTION_VALIDATE(color_value->GetInteger(i, &color_array[i]));

  SkColor color = SkColorSetARGB(color_array[3], color_array[0], color_array[1],
                                 color_array[2]);
  page_action_->SetBadgeTextColor(tab_id, color);
  contents_->extension_tab_helper()->PageActionStateChanged();
  return true;
}

// Not currently exposed to extensions. To re-enable, add mapping in
// extension_function_dispatcher.
bool PageActionSetBadgeTextFunction::RunImpl() {
  DictionaryValue* args;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger("tabId", &tab_id));
  if (!InitCommon(tab_id))
    return false;

  std::string text;
  EXTENSION_FUNCTION_VALIDATE(args->GetString("text", &text));

  page_action_->SetBadgeText(tab_id, text);
  contents_->extension_tab_helper()->PageActionStateChanged();
  return true;
}
