// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/app_shim_handler_mac.h"

#include <map>

#include "apps/shell_window_registry.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

namespace apps {

namespace {

void TerminateIfNoShellWindows() {
  bool shell_windows_left =
      apps::ShellWindowRegistry::IsShellWindowRegisteredInAnyProfile(0);
  if (!shell_windows_left && !AppListService::Get(
          chrome::HOST_DESKTOP_TYPE_NATIVE)->IsAppListVisible()) {
    chrome::AttemptExit();
  }
}

class AppShimHandlerRegistry : public content::NotificationObserver {
 public:
  static AppShimHandlerRegistry* GetInstance() {
    return Singleton<AppShimHandlerRegistry,
                     LeakySingletonTraits<AppShimHandlerRegistry> >::get();
  }

  AppShimHandler* GetForAppMode(const std::string& app_mode_id) const {
    HandlerMap::const_iterator it = handlers_.find(app_mode_id);
    if (it != handlers_.end())
      return it->second;

    return default_handler_;
  }

  bool SetForAppMode(const std::string& app_mode_id, AppShimHandler* handler) {
    bool inserted_or_removed = handler ?
        handlers_.insert(HandlerMap::value_type(app_mode_id, handler)).second :
        handlers_.erase(app_mode_id) == 1;
    DCHECK(inserted_or_removed);
    return inserted_or_removed;
  }

  void SetDefaultHandler(AppShimHandler* handler) {
    DCHECK_NE(default_handler_ == NULL, handler == NULL);
    default_handler_ = handler;
  }

  void MaybeTerminate() {
    if (!browser_opened_ever_) {
      // Post this to give ShellWindows a chance to remove themselves from the
      // registry.
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&TerminateIfNoShellWindows));
    }
  }

 private:
  friend struct DefaultSingletonTraits<AppShimHandlerRegistry>;
  typedef std::map<std::string, AppShimHandler*> HandlerMap;

  AppShimHandlerRegistry()
      : default_handler_(NULL),
        browser_opened_ever_(false) {
    registrar_.Add(
        this, chrome::NOTIFICATION_BROWSER_OPENED,
        content::NotificationService::AllBrowserContextsAndSources());
  }

  virtual ~AppShimHandlerRegistry() {}

  // content::NotificationObserver override:
  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE {
    DCHECK_EQ(chrome::NOTIFICATION_BROWSER_OPENED, type);
    registrar_.Remove(
        this, chrome::NOTIFICATION_BROWSER_OPENED,
        content::NotificationService::AllBrowserContextsAndSources());
    browser_opened_ever_ = true;
  }

  HandlerMap handlers_;
  AppShimHandler* default_handler_;
  content::NotificationRegistrar registrar_;
  bool browser_opened_ever_;

  DISALLOW_COPY_AND_ASSIGN(AppShimHandlerRegistry);
};

}  // namespace

// static
void AppShimHandler::RegisterHandler(const std::string& app_mode_id,
                                     AppShimHandler* handler) {
  DCHECK(handler);
  AppShimHandlerRegistry::GetInstance()->SetForAppMode(app_mode_id, handler);
}

// static
void AppShimHandler::RemoveHandler(const std::string& app_mode_id) {
  AppShimHandlerRegistry::GetInstance()->SetForAppMode(app_mode_id, NULL);
}

// static
AppShimHandler* AppShimHandler::GetForAppMode(const std::string& app_mode_id) {
  return AppShimHandlerRegistry::GetInstance()->GetForAppMode(app_mode_id);
}

// static
void AppShimHandler::SetDefaultHandler(AppShimHandler* handler) {
  AppShimHandlerRegistry::GetInstance()->SetDefaultHandler(handler);
}

// static
void AppShimHandler::MaybeTerminate() {
  AppShimHandlerRegistry::GetInstance()->MaybeTerminate();
}

}  // namespace apps
