// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_shim/app_shim_handler_mac.h"

#include <map>

#include "base/logging.h"
#include "base/memory/singleton.h"

namespace apps {

namespace {

class AppShimHandlerRegistry {
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

 private:
  friend struct DefaultSingletonTraits<AppShimHandlerRegistry>;
  typedef std::map<std::string, AppShimHandler*> HandlerMap;

  AppShimHandlerRegistry() : default_handler_(NULL) {}
  ~AppShimHandlerRegistry() {}

  HandlerMap handlers_;
  AppShimHandler* default_handler_;

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

}  // namespace apps
