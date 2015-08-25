// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SUPERVISED_USER_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SUPERVISED_USER_INTERNALS_MESSAGE_HANDLER_H_

#include "base/callback_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

class SupervisedUserService;

// The implementation for the chrome://supervised-user-internals page.
class SupervisedUserInternalsMessageHandler
    : public content::WebUIMessageHandler,
      public SupervisedUserServiceObserver {
 public:
  SupervisedUserInternalsMessageHandler();
  ~SupervisedUserInternalsMessageHandler() override;

 private:
  class IOThreadHelper;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // SupervisedUserServiceObserver:
  void OnURLFilterChanged() override;

  SupervisedUserService* GetSupervisedUserService();

  void HandleRegisterForEvents(const base::ListValue* args);
  void HandleGetBasicInfo(const base::ListValue* args);
  void HandleTryURL(const base::ListValue* args);

  void SendBasicInfo();
  void SendSupervisedUserSettings(const base::DictionaryValue* settings);

  void OnTryURLResult(
      SupervisedUserURLFilter::FilteringBehavior behavior,
      SupervisedUserURLFilter::FilteringBehaviorReason reason,
      bool uncertain);

  void OnURLChecked(const GURL& url,
                    SupervisedUserURLFilter::FilteringBehavior behavior,
                    SupervisedUserURLFilter::FilteringBehaviorReason reason,
                    bool uncertain);

  scoped_ptr<base::CallbackList<void(
      const base::DictionaryValue*)>::Subscription> user_settings_subscription_;

  scoped_refptr<IOThreadHelper> io_thread_helper_;

  base::WeakPtrFactory<SupervisedUserInternalsMessageHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserInternalsMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SUPERVISED_USER_INTERNALS_MESSAGE_HANDLER_H_
