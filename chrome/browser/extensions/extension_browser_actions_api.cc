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

bool BrowserActionFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->IsType(Value::TYPE_DICTIONARY));
  details_ = static_cast<DictionaryValue*>(args_);

  if (details_->HasKey(L"tabId"))
    EXTENSION_FUNCTION_VALIDATE(details_->GetInteger(L"tabId", &tab_id_));

  Extension* extension = dispatcher()->GetExtension();
  browser_action_ = extension->browser_action();
  if (!browser_action_) {
    error_ = kNoBrowserActionError;
    return false;
  }

  if (!RunBrowserAction())
    return false;

  NotificationService::current()->Notify(
      NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
      Source<ExtensionAction2>(browser_action_),
      NotificationService::NoDetails());
  return true;
}

bool BrowserActionSetIconFunction::RunBrowserAction() {
  BinaryValue* binary = NULL;
  EXTENSION_FUNCTION_VALIDATE(details_->GetBinary(L"imageData", &binary));
  IPC::Message bitmap_pickle(binary->GetBuffer(), binary->GetSize());
  void* iter = NULL;
  SkBitmap bitmap;
  EXTENSION_FUNCTION_VALIDATE(
      IPC::ReadParam(&bitmap_pickle, &iter, &bitmap));
  browser_action_->SetIcon(tab_id_, bitmap);
  return true;
}

bool BrowserActionSetTitleFunction::RunBrowserAction() {
  std::string title;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString(L"title", &title));
  browser_action_->SetTitle(tab_id_, title);
  return true;
}

bool BrowserActionSetBadgeTextFunction::RunBrowserAction() {
  std::string badge_text;
  EXTENSION_FUNCTION_VALIDATE(details_->GetString(L"text", &badge_text));
  browser_action_->SetBadgeText(tab_id_, badge_text);
  return true;
}

bool BrowserActionSetBadgeBackgroundColorFunction::RunBrowserAction() {
  ListValue* list = NULL;
  EXTENSION_FUNCTION_VALIDATE(details_->GetList(L"color", &list));
  EXTENSION_FUNCTION_VALIDATE(list->GetSize() == 4);

  int color_array[4] = {0};
  for (size_t i = 0; i < arraysize(color_array); ++i) {
    EXTENSION_FUNCTION_VALIDATE(list->GetInteger(i, &color_array[i]));
  }

  SkColor color = SkColorSetARGB(color_array[3], color_array[0], color_array[1],
                                 color_array[2]);
  browser_action_->SetBadgeBackgroundColor(tab_id_, color);

  return true;
}
