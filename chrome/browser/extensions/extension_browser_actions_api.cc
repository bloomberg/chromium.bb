// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browser_actions_api.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/render_messages.h"

namespace {
// Errors.
const char kNoBrowserActionError[] =
    "This extension has no browser action specified.";
const char kIconIndexOutOfBounds[] =
    "Browser action icon index out of bounds.";
}

bool BrowserActionSetIconFunction::RunImpl() {
  // setIcon can take a variant argument: either a canvas ImageData, or an
  // icon index.
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_BINARY) ||
                              args_->IsType(Value::TYPE_DICTIONARY));

  Extension* extension = dispatcher()->GetExtension();
  if (!extension->browser_action()) {
    error_ = kNoBrowserActionError;
    return false;
  }

  if (args_->IsType(Value::TYPE_BINARY)) {
    BinaryValue* binary = static_cast<BinaryValue*>(args_);
    IPC::Message bitmap_pickle(binary->GetBuffer(), binary->GetSize());
    void* iter = NULL;
    scoped_ptr<SkBitmap> bitmap(new SkBitmap);
    EXTENSION_FUNCTION_VALIDATE(
        IPC::ReadParam(&bitmap_pickle, &iter, bitmap.get()));
    extension->browser_action_state()->set_icon(bitmap.release());
  } else {
    int icon_index = -1;
    EXTENSION_FUNCTION_VALIDATE(
        static_cast<DictionaryValue*>(args_)->GetInteger(
            L"iconIndex", &icon_index));
    if (icon_index < 0 ||
        static_cast<size_t>(icon_index) >=
            extension->browser_action()->icon_paths().size()) {
      error_ = kIconIndexOutOfBounds;
      return false;
    }
    extension->browser_action_state()->set_icon_index(icon_index);
    extension->browser_action_state()->set_icon(NULL);
  }

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
      Source<ExtensionAction>(extension->browser_action()),
      Details<ExtensionActionState>(extension->browser_action_state()));
  return true;
}

bool BrowserActionSetTitleFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  DictionaryValue* details = static_cast<DictionaryValue*>(args_);

  std::string title;
  EXTENSION_FUNCTION_VALIDATE(details->GetString(L"title", &title));

  Extension* extension = dispatcher()->GetExtension();
  if (!extension->browser_action()) {
    error_ = kNoBrowserActionError;
    return false;
  }

  extension->browser_action_state()->set_title(title);

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
      Source<ExtensionAction>(extension->browser_action()),
      Details<ExtensionActionState>(extension->browser_action_state()));
  return true;
}

bool BrowserActionSetBadgeTextFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  DictionaryValue* details = static_cast<DictionaryValue*>(args_);

  std::string badge_text;
  EXTENSION_FUNCTION_VALIDATE(details->GetString(L"text", &badge_text));

  Extension* extension = dispatcher()->GetExtension();
  if (!extension->browser_action()) {
    error_ = kNoBrowserActionError;
    return false;
  }

  extension->browser_action_state()->set_badge_text(badge_text);

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
      Source<ExtensionAction>(extension->browser_action()),
      Details<ExtensionActionState>(extension->browser_action_state()));
  return true;
}

bool BrowserActionSetBadgeBackgroundColorFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  DictionaryValue* details = static_cast<DictionaryValue*>(args_);

  ListValue* list = NULL;
  EXTENSION_FUNCTION_VALIDATE(details->GetList(L"color", &list));
  EXTENSION_FUNCTION_VALIDATE(list->GetSize() == 4);

  int color_array[4] = {0};
  for (size_t i = 0; i < arraysize(color_array); ++i) {
    EXTENSION_FUNCTION_VALIDATE(list->GetInteger(i, &color_array[i]));
  }

  SkColor color = SkColorSetARGB(color_array[0], color_array[1], color_array[2],
                                 color_array[3]);

  Extension* extension = dispatcher()->GetExtension();
  if (!extension->browser_action()) {
    error_ = kNoBrowserActionError;
    return false;
  }

  extension->browser_action_state()->set_badge_background_color(color);

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
      Source<ExtensionAction>(extension->browser_action()),
      Details<ExtensionActionState>(extension->browser_action_state()));
  return true;
}
