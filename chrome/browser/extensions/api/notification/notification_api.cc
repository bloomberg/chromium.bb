// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notification/notification_api.h"

#include "base/callback.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/api_resource_event_notifier.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/common/extensions/extension.h"
#include "googleurl/src/gurl.h"

const char kResultKey[] = "result";

namespace {

class NotificationApiDelegate : public NotificationDelegate {
 public:
  explicit NotificationApiDelegate(
      extensions::ApiResourceEventNotifier* event_notifier)
      : event_notifier_(event_notifier) {
  }

  virtual void Display() OVERRIDE {
    // TODO(miket): propagate to JS
  }

  virtual void Error() OVERRIDE {
    // TODO(miket): propagate to JS
  }

  virtual void Close(bool by_user) OVERRIDE {
    // TODO(miket): propagate to JS
  }

  virtual void Click() OVERRIDE {
    // TODO(miket): propagate to JS
  }

  virtual std::string id() const OVERRIDE {
    // TODO(miket): implement
    return std::string();
  }

  virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE {
    // TODO(miket): required to handle icon
    return NULL;
  }

 private:
  virtual ~NotificationApiDelegate() {}

  extensions::ApiResourceEventNotifier* event_notifier_;

  DISALLOW_COPY_AND_ASSIGN(NotificationApiDelegate);
};

}  // namespace

namespace extensions {

NotificationShowFunction::NotificationShowFunction() {
}

NotificationShowFunction::~NotificationShowFunction() {
}

bool NotificationShowFunction::RunImpl() {
  params_ = api::experimental_notification::Show::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  api::experimental_notification::ShowOptions* options = &params_->options;
  scoped_ptr<DictionaryValue> options_dict(options->ToValue());
  src_id_ = ExtractSrcId(options_dict.get());
  event_notifier_ = CreateEventNotifier(src_id_);

  GURL icon_url(UTF8ToUTF16(options->icon_url));
  string16 title(UTF8ToUTF16(options->title));
  string16 message(UTF8ToUTF16(options->message));
  string16 replace_id(UTF8ToUTF16(options->replace_id));

  Notification notification(
      GURL(), icon_url, title, message, WebKit::WebTextDirectionDefault,
      string16(), replace_id,
      new NotificationApiDelegate(event_notifier_));
  g_browser_process->notification_ui_manager()->Add(notification, profile());

  // TODO(miket): why return a result if it's always true?
  DictionaryValue* result = new DictionaryValue();
  result->SetBoolean(kResultKey, true);
  SetResult(result);
  SendResponse(true);

  return true;
}

}  // namespace extensions
