// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_app_api.h"

#include "base/values.h"
#include "base/time.h"
#include "chrome/browser/extensions/app_notification_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"

const char kBodyTextKey[] = "bodyText";
const char kExtensionIdKey[] = "extensionId";
const char kLinkTextKey[] = "linkText";
const char kLinkUrlKey[] = "linkUrl";
const char kTitleKey[] = "title";

const char kInvalidExtensionIdError[] =
    "Invalid extension id";
const char kMissingLinkTextError[] =
    "You must specify linkText if you use linkUrl";

bool AppNotifyFunction::RunImpl() {
  DictionaryValue* details;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &details));
  EXTENSION_FUNCTION_VALIDATE(details != NULL);

  // TODO(asargent) remove this before the API leaves experimental.
  std::string id = extension_id();
  if (details->HasKey(kExtensionIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(details->GetString(kExtensionIdKey, &id));
    if (!profile()->GetExtensionService()->GetExtensionById(id, true)) {
      error_ = kInvalidExtensionIdError;
      return false;
    }
  }

  std::string title;
  if (details->HasKey(kTitleKey))
    EXTENSION_FUNCTION_VALIDATE(details->GetString(kTitleKey, &title));

  std::string body;
  if (details->HasKey(kBodyTextKey))
    EXTENSION_FUNCTION_VALIDATE(details->GetString(kBodyTextKey, &body));

  scoped_ptr<AppNotification> item(new AppNotification(
      true, base::Time::Now(), "", id, title, body));

  if (details->HasKey(kLinkUrlKey)) {
    std::string link_url;
    EXTENSION_FUNCTION_VALIDATE(details->GetString(kLinkUrlKey, &link_url));
    item->set_link_url(GURL(link_url));
    if (!item->link_url().is_valid()) {
      error_ = "Invalid url: " + link_url;
      return false;
    }
    if (!details->HasKey(kLinkTextKey)) {
      error_ = kMissingLinkTextError;
      return false;
    }
    std::string link_text;
    EXTENSION_FUNCTION_VALIDATE(details->GetString(kLinkTextKey,
                                                   &link_text));
    item->set_link_text(link_text);
  }

  AppNotificationManager* manager =
      profile()->GetExtensionService()->app_notification_manager();

  manager->Add(item.release());

  return true;
}

bool AppClearAllNotificationsFunction::RunImpl() {
  std::string id = extension_id();
  DictionaryValue* details = NULL;
  if (args_->GetDictionary(0, &details) && details->HasKey(kExtensionIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(details->GetString(kExtensionIdKey, &id));
    if (!profile()->GetExtensionService()->GetExtensionById(id, true)) {
      error_ = kInvalidExtensionIdError;
      return false;
    }
  }

  AppNotificationManager* manager =
      profile()->GetExtensionService()->app_notification_manager();
  manager->ClearAll(id);
  return true;
}
