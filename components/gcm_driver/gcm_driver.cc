// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/gcm_driver.h"

#include "base/logging.h"
#include "components/gcm_driver/gcm_app_handler.h"

namespace gcm {

GCMDriver::GCMDriver() {
}

GCMDriver::~GCMDriver() {
}

void GCMDriver::Shutdown() {
  for (GCMAppHandlerMap::const_iterator iter = app_handlers_.begin();
       iter != app_handlers_.end(); ++iter) {
    iter->second->ShutdownHandler();
  }
  app_handlers_.clear();
}

void GCMDriver::AddAppHandler(const std::string& app_id,
                              GCMAppHandler* handler) {
  DCHECK(!app_id.empty());
  DCHECK(handler);
  DCHECK(app_handlers_.find(app_id) == app_handlers_.end());
  app_handlers_[app_id] = handler;
}

void GCMDriver::RemoveAppHandler(const std::string& app_id) {
  DCHECK(!app_id.empty());
  app_handlers_.erase(app_id);
}

GCMAppHandler* GCMDriver::GetAppHandler(const std::string& app_id) {
  GCMAppHandlerMap::const_iterator iter = app_handlers_.find(app_id);
  return iter == app_handlers_.end() ? &default_app_handler_ : iter->second;
}

}  // namespace gcm
