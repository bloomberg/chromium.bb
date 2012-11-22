// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/user_actions/user_actions_ui_handler.h"

#include "base/logging.h"
#include "base/values.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_ui.h"

UserActionsUIHandler::UserActionsUIHandler() : NotificationObserver() {
  registrar_.Add(this,
                 content::NOTIFICATION_USER_ACTION,
                 content::NotificationService::AllSources());
}

UserActionsUIHandler::~UserActionsUIHandler() {
}

void UserActionsUIHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_USER_ACTION);
  base::StringValue user_action_name(
      *content::Details<const char*>(details).ptr());
  web_ui()->CallJavascriptFunction("userActions.observeUserAction",
                                   user_action_name);
}

void UserActionsUIHandler::RegisterMessages() {}
