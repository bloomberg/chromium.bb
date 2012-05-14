// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/extension_actions_api.h"

#include <string>

#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/extension_action/extension_page_actions_api_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"

namespace {

// Errors.
const char kNoExtensionActionError[] =
    "This extension has no action specified.";
const char kNoTabError[] = "No tab with id: *.";
const char kIconIndexOutOfBounds[] = "Page action icon index out of bounds.";

}

ExtensionActionFunction::ExtensionActionFunction()
    : details_(NULL),
      tab_id_(ExtensionAction::kDefaultTabId),
      contents_(NULL),
      extension_action_(NULL) {
}

ExtensionActionFunction::~ExtensionActionFunction() {
}

bool ExtensionActionFunction::RunImpl() {
  extension_action_ = GetExtension()->browser_action();
  if (!extension_action_)
    extension_action_ = GetExtension()->page_action();
  EXTENSION_FUNCTION_VALIDATE(extension_action_);

  // There may or may not be details (depends on the function).
  // The tabId might appear in details (if it exists) or as the first
  // argument besides the action type (depends on the function).
  {
    base::Value* first_arg = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &first_arg));

    switch (first_arg->GetType()) {
      case Value::TYPE_INTEGER:
        CHECK(first_arg->GetAsInteger(&tab_id_));
        break;

      case Value::TYPE_DICTIONARY: {
        details_ = static_cast<base::DictionaryValue*>(first_arg);
        base::Value* tab_id_value = NULL;
        if (details_->Get("tabId", &tab_id_value)) {
          switch (tab_id_value->GetType()) {
            case Value::TYPE_NULL:
              // Fine, equivalent to it being not-there, and tabId is optional.
              break;
            case Value::TYPE_INTEGER:
              CHECK(tab_id_value->GetAsInteger(&tab_id_));
              break;
            default:
              // Boom.
              EXTENSION_FUNCTION_VALIDATE(false);
          }
        }
        break;
      }

      default:
        EXTENSION_FUNCTION_VALIDATE(false);
    }
  }

  // Find the TabContentsWrapper that contains this tab id if one is required.
  if (tab_id_ == ExtensionAction::kDefaultTabId) {
    EXTENSION_FUNCTION_VALIDATE(GetExtension()->browser_action());
  } else {
    ExtensionTabUtil::GetTabById(
        tab_id_, profile(), include_incognito(), NULL, NULL, &contents_, NULL);
    if (!contents_) {
      error_ = ExtensionErrorUtils::FormatErrorMessage(
          kNoTabError, base::IntToString(tab_id_));
      return false;
    }
  }

  return RunExtensionAction();
}

void ExtensionActionFunction::NotifyChange() {
  if (GetExtension()->browser_action())
    NotifyBrowserActionChange();
  else if (GetExtension()->page_action())
    NotifyPageActionChange();
  else
    NOTREACHED();
}

void ExtensionActionFunction::NotifyBrowserActionChange() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED,
      content::Source<ExtensionAction>(extension_action_),
      content::NotificationService::NoDetails());
}

void ExtensionActionFunction::NotifyPageActionChange() {
  contents_->extension_tab_helper()->PageActionStateChanged();
}

// static
bool ExtensionActionFunction::ParseCSSColorString(
    const std::string& color_string,
    SkColor* result) {
  std::string formatted_color = "#";
  // Check the string for incorrect formatting.
  if (color_string[0] != '#')
    return false;

  // Convert the string from #FFF format to #FFFFFF format.
  if (color_string.length() == 4) {
    for (size_t i = 1; i < color_string.length(); i++) {
      formatted_color += color_string[i];
      formatted_color += color_string[i];
    }
  } else {
    formatted_color = color_string;
  }

  if (formatted_color.length() != 7)
    return false;

  // Convert the string to an integer and make sure it is in the correct value
  // range.
  int color_ints[3] = {0};
  for (int i = 0; i < 3; i++) {
    if (!base::HexStringToInt(formatted_color.substr(1 + (2 * i), 2),
                              color_ints + i))
      return false;
    if (color_ints[i] > 255 || color_ints[i] < 0)
      return false;
  }

  *result = SkColorSetARGB(255, color_ints[0], color_ints[1], color_ints[2]);
  return true;
}

bool ExtensionActionFunction::SetVisible(bool visible) {
  // If --browser-actions-for-all is enabled there will be a browser_action
  // here instead of a page action. Until we decide what to do with that, just
  // ignore.
  if (!GetExtension()->page_action())
    return true;
  extension_action_->SetIsVisible(tab_id_, visible);
  NotifyChange();
  return true;
}

bool ExtensionActionShowFunction::RunExtensionAction() {
  return SetVisible(true);
}

bool ExtensionActionHideFunction::RunExtensionAction() {
  return SetVisible(false);
}

bool ExtensionActionSetIconFunction::RunExtensionAction() {
  // setIcon can take a variant argument: either a canvas ImageData, or an
  // icon index.
  base::BinaryValue* binary = NULL;
  int icon_index;
  if (details_->GetBinary("imageData", &binary)) {
    IPC::Message bitmap_pickle(binary->GetBuffer(), binary->GetSize());
    PickleIterator iter(bitmap_pickle);
    SkBitmap bitmap;
    EXTENSION_FUNCTION_VALIDATE(
        IPC::ReadParam(&bitmap_pickle, &iter, &bitmap));
    extension_action_->SetIcon(tab_id_, bitmap);
  } else if (details_->GetInteger("iconIndex", &icon_index)) {
    // If --browser-actions-for-all is enabled there might legitimately be an
    // iconIndex set. Until we decide what to do with that, ignore.
    if (!GetExtension()->page_action())
      return true;
    if (icon_index < 0 ||
        static_cast<size_t>(icon_index) >=
            extension_action_->icon_paths()->size()) {
      error_ = kIconIndexOutOfBounds;
      return false;
    }
    extension_action_->SetIcon(tab_id_, SkBitmap());
    extension_action_->SetIconIndex(tab_id_, icon_index);
  } else {
    EXTENSION_FUNCTION_VALIDATE(false);
  }
  NotifyChange();
  return true;
}

bool ExtensionActionSetTitleFunction::RunExtensionAction() {
  std::string title;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString("title", &title));
  extension_action_->SetTitle(tab_id_, title);
  NotifyChange();
  return true;
}

bool ExtensionActionSetPopupFunction::RunExtensionAction() {
  std::string popup_string;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString("popup", &popup_string));

  GURL popup_url;
  if (!popup_string.empty())
    popup_url = GetExtension()->GetResourceURL(popup_string);

  extension_action_->SetPopupUrl(tab_id_, popup_url);
  NotifyChange();
  return true;
}

bool ExtensionActionSetBadgeTextFunction::RunExtensionAction() {
  std::string badge_text;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString("text", &badge_text));
  extension_action_->SetBadgeText(tab_id_, badge_text);
  NotifyChange();
  return true;
}

bool ExtensionActionSetBadgeBackgroundColorFunction::RunExtensionAction() {
  Value* color_value = NULL;
  details_->Get("color", &color_value);
  SkColor color = 0;
  if (color_value->IsType(Value::TYPE_LIST)) {
    ListValue* list = NULL;
    EXTENSION_FUNCTION_VALIDATE(details_->GetList("color", &list));
    EXTENSION_FUNCTION_VALIDATE(list->GetSize() == 4);

    int color_array[4] = {0};
    for (size_t i = 0; i < arraysize(color_array); ++i) {
      EXTENSION_FUNCTION_VALIDATE(list->GetInteger(i, &color_array[i]));
    }

    color = SkColorSetARGB(color_array[3], color_array[0],
                           color_array[1], color_array[2]);
  } else if (color_value->IsType(Value::TYPE_STRING)) {
    std::string color_string;
    EXTENSION_FUNCTION_VALIDATE(details_->GetString("color", &color_string));
    if (!ParseCSSColorString(color_string, &color))
      return false;
  }

  extension_action_->SetBadgeBackgroundColor(tab_id_, color);
  NotifyChange();
  return true;
}

bool ExtensionActionGetTitleFunction::RunExtensionAction() {
  result_.reset(Value::CreateStringValue(extension_action_->GetTitle(tab_id_)));
  return true;
}

bool ExtensionActionGetPopupFunction::RunExtensionAction() {
  result_.reset(Value::CreateStringValue(
                    extension_action_->GetPopupUrl(tab_id_).spec()));
  return true;
}

bool ExtensionActionGetBadgeTextFunction::RunExtensionAction() {
  result_.reset(Value::CreateStringValue(
                    extension_action_->GetBadgeText(tab_id_)));
  return true;
}

bool ExtensionActionGetBadgeBackgroundColorFunction::RunExtensionAction() {
  ListValue* list = new ListValue();
  SkColor color = extension_action_->GetBadgeBackgroundColor(tab_id_);
  list->Append(Value::CreateIntegerValue(SkColorGetR(color)));
  list->Append(Value::CreateIntegerValue(SkColorGetG(color)));
  list->Append(Value::CreateIntegerValue(SkColorGetB(color)));
  list->Append(Value::CreateIntegerValue(SkColorGetA(color)));
  result_.reset(list);
  return true;
}
