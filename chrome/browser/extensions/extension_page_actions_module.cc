// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_page_actions_module.h"

#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/extensions/extension_page_actions_module_constants.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/render_messages.h"

namespace keys = extension_page_actions_module_constants;

namespace {
// Errors.
const char kNoExtensionError[] = "No extension with id: *.";
const char kNoTabError[] = "No tab with id: *.";
const char kNoPageActionError[] =
    "This extension has no page action specified.";
const char kUrlNotActiveError[] = "This url is no longer active: *.";
const char kIconIndexOutOfBounds[] = "Page action icon index out of bounds.";
}

// TODO(EXTENSIONS_DEPRECATED): obsolete API.
bool PageActionFunction::SetPageActionEnabled(bool enable) {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_LIST));
  const ListValue* args = static_cast<const ListValue*>(args_);

  std::string page_action_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetString(0, &page_action_id));
  DictionaryValue* action;
  EXTENSION_FUNCTION_VALIDATE(args->GetDictionary(1, &action));

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

  // Find the TabContents that contains this tab id.
  TabContents* contents = NULL;
  ExtensionTabUtil::GetTabById(tab_id, profile(), NULL, NULL, &contents, NULL);
  if (!contents) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(kNoTabError,
                                                     IntToString(tab_id));
    return false;
  }

  // Make sure the URL hasn't changed.
  NavigationEntry* entry = contents->controller().GetActiveEntry();
  if (!entry || url != entry->url().spec()) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(kUrlNotActiveError, url);
    return false;
  }

  // Find our extension.
  Extension* extension = NULL;
  ExtensionsService* service = profile()->GetExtensionsService();
  extension = service->GetExtensionById(extension_id());
  if (!extension) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(kNoExtensionError,
                                                     extension_id());
    return false;
  }

  const ExtensionAction* page_action = extension->page_action();
  if (!page_action) {
    error_ = kNoPageActionError;
    return false;
  }

  // Set visibility and broadcast notifications that the UI should be updated.
  contents->SetPageActionEnabled(page_action, enable, title, icon_id);
  contents->PageActionStateChanged();

  return true;
}

bool PageActionFunction::InitCommon(int tab_id) {
  page_action_ = dispatcher()->GetExtension()->page_action();
  if (!page_action_) {
    error_ = kNoPageActionError;
    return false;
  }

  // Find the TabContents that contains this tab id.
  contents_ = NULL;
  ExtensionTabUtil::GetTabById(tab_id, profile(), NULL, NULL, &contents_, NULL);
  if (!contents_) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(kNoTabError,
                                                     IntToString(tab_id));
    return false;
  }

  state_ = contents_->GetOrCreatePageActionState(page_action_);
  return true;
}

bool PageActionFunction::SetHidden(bool hidden) {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetAsInteger(&tab_id));
  if (!InitCommon(tab_id))
    return false;

  state_->set_hidden(hidden);
  contents_->PageActionStateChanged();
  return true;
}

bool EnablePageActionFunction::RunImpl() {
  return SetPageActionEnabled(true);
}

bool DisablePageActionFunction::RunImpl() {
  return SetPageActionEnabled(false);
}

bool PageActionShowFunction::RunImpl() {
  return SetHidden(false);
}

bool PageActionHideFunction::RunImpl() {
  return SetHidden(true);
}

bool PageActionSetIconFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue* args = static_cast<const DictionaryValue*>(args_);

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(L"tabId", &tab_id));
  if (!InitCommon(tab_id))
    return false;

  // setIcon can take a variant argument: either a canvas ImageData, or an
  // icon index.
  BinaryValue* binary;
  int icon_index;
  if (args->GetBinary(L"imageData", &binary)) {
    IPC::Message bitmap_pickle(binary->GetBuffer(), binary->GetSize());
    void* iter = NULL;
    scoped_ptr<SkBitmap> bitmap(new SkBitmap);
    EXTENSION_FUNCTION_VALIDATE(
        IPC::ReadParam(&bitmap_pickle, &iter, bitmap.get()));
    state_->set_icon(bitmap.release());
  } else if (args->GetInteger(L"iconIndex", &icon_index)) {
    if (icon_index < 0 ||
        static_cast<size_t>(icon_index) >= page_action_->icon_paths().size()) {
      error_ = kIconIndexOutOfBounds;
      return false;
    }
    state_->set_icon(NULL);
    state_->set_icon_index(icon_index);
  } else {
    EXTENSION_FUNCTION_VALIDATE(false);
  }

  contents_->PageActionStateChanged();
  return true;
}

bool PageActionSetTitleFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue* args = static_cast<const DictionaryValue*>(args_);

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(L"tabId", &tab_id));
  if (!InitCommon(tab_id))
    return false;

  std::string title;
  EXTENSION_FUNCTION_VALIDATE(args->GetString(L"title", &title));

  state_->set_title(title);
  contents_->PageActionStateChanged();
  return true;
}

bool PageActionSetBadgeBackgroundColorFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue* args = static_cast<const DictionaryValue*>(args_);

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(L"tabId", &tab_id));
  if (!InitCommon(tab_id))
    return false;

  ListValue* color_value;
  EXTENSION_FUNCTION_VALIDATE(args->GetList(L"color", &color_value));
  EXTENSION_FUNCTION_VALIDATE(color_value->GetSize() == 4);

  int color_array[4] = {0};
  for (size_t i = 0; i < arraysize(color_array); ++i)
    EXTENSION_FUNCTION_VALIDATE(color_value->GetInteger(i, &color_array[i]));

  SkColor color = SkColorSetARGB(color_array[0], color_array[1], color_array[2],
                                 color_array[3]);
  state_->set_badge_background_color(color);
  contents_->PageActionStateChanged();
  return true;
}

bool PageActionSetBadgeTextColorFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue* args = static_cast<const DictionaryValue*>(args_);

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(L"tabId", &tab_id));
  if (!InitCommon(tab_id))
    return false;

  ListValue* color_value;
  EXTENSION_FUNCTION_VALIDATE(args->GetList(L"color", &color_value));
  EXTENSION_FUNCTION_VALIDATE(color_value->GetSize() == 4);

  int color_array[4] = {0};
  for (size_t i = 0; i < arraysize(color_array); ++i)
    EXTENSION_FUNCTION_VALIDATE(color_value->GetInteger(i, &color_array[i]));

  // TODO(mpcomplete): implement text coloring.
  return true;
}

bool PageActionSetBadgeTextFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  const DictionaryValue* args = static_cast<const DictionaryValue*>(args_);

  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args->GetInteger(L"tabId", &tab_id));
  if (!InitCommon(tab_id))
    return false;

  std::string text;
  EXTENSION_FUNCTION_VALIDATE(args->GetString(L"text", &text));

  state_->set_badge_text(text);
  contents_->PageActionStateChanged();
  return true;
}
