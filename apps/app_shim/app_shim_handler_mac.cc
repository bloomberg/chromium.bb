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

    return NULL;
  }

  bool SetForAppMode(const std::string& app_mode_id, AppShimHandler* handler) {
    bool inserted_or_removed = handler ?
        handlers_.insert(HandlerMap::value_type(app_mode_id, handler)).second :
        handlers_.erase(app_mode_id) == 1;
    DCHECK(inserted_or_removed);
    return inserted_or_removed;
  }

 private:
  friend struct DefaultSingletonTraits<AppShimHandlerRegistry>;
  typedef std::map<std::string, AppShimHandler*> HandlerMap;

  AppShimHandlerRegistry() {}
  ~AppShimHandlerRegistry() {}

  HandlerMap handlers_;

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

}  // namespace apps
