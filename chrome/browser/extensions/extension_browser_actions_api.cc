// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browser_actions_api.h"

#include <string>

#include "base/values.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/notification_service.h"

namespace {
// Errors.
const char kNoBrowserActionError[] =
    "This extension has no browser action specified.";
const char kIconIndexOutOfBounds[] =
    "Browser action icon index out of bounds.";
}

bool BrowserActionFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details_));
  EXTENSION_FUNCTION_VALIDATE(details_ != NULL);

  if (details_->HasKey("tabId"))
    EXTENSION_FUNCTION_VALIDATE(details_->GetInteger("tabId", &tab_id_));

  const Extension* extension = GetExtension();
  browser_action_ = extension->browser_action();
  if (!browser_action_) {
    error_ = kNoBrowserActionError;
    return false;
  }

  if (!RunBrowserAction())
    return false;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED,
      content::Source<ExtensionAction>(browser_action_),
      content::NotificationService::NoDetails());
  return true;
}

bool BrowserActionSetIconFunction::RunBrowserAction() {
  base::BinaryValue* binary = NULL;
  EXTENSION_FUNCTION_VALIDATE(details_->GetBinary("imageData", &binary));
  IPC::Message bitmap_pickle(binary->GetBuffer(), binary->GetSize());
  PickleIterator iter(bitmap_pickle);
  SkBitmap bitmap;
  EXTENSION_FUNCTION_VALIDATE(
      IPC::ReadParam(&bitmap_pickle, &iter, &bitmap));
  browser_action_->SetIcon(tab_id_, bitmap);
  return true;
}

bool BrowserActionSetTitleFunction::RunBrowserAction() {
  std::string title;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString("title", &title));
  browser_action_->SetTitle(tab_id_, title);
  return true;
}

bool BrowserActionSetPopupFunction::RunBrowserAction() {
  std::string popup_string;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString("popup", &popup_string));

  GURL popup_url;
  if (!popup_string.empty())
    popup_url = GetExtension()->GetResourceURL(popup_string);

  browser_action_->SetPopupUrl(tab_id_, popup_url);
  return true;
}

bool BrowserActionSetBadgeTextFunction::RunBrowserAction() {
  std::string badge_text;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString("text", &badge_text));
  browser_action_->SetBadgeText(tab_id_, badge_text);
  return true;
}

bool BrowserActionFunction::ParseCSSColorString(const std::string& color_string,
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

bool BrowserActionSetBadgeBackgroundColorFunction::RunBrowserAction() {
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

  browser_action_->SetBadgeBackgroundColor(tab_id_, color);

  return true;
}
