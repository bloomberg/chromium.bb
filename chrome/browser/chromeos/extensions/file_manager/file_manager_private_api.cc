// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_manager_private_api.h"

#include "chrome/browser/chromeos/extensions/file_manager/event_router.h"
#include "chrome/browser/chromeos/extensions/file_manager/file_manager_private_api_factory.h"

namespace file_manager {

FileManagerPrivateAPI::FileManagerPrivateAPI(Profile* profile)
    : event_router_(new EventRouter(profile)) {
  event_router_->ObserveEvents();
}

FileManagerPrivateAPI::~FileManagerPrivateAPI() {
}

void FileManagerPrivateAPI::Shutdown() {
  event_router_->Shutdown();
}

// static
FileManagerPrivateAPI* FileManagerPrivateAPI::Get(Profile* profile) {
  return FileManagerPrivateAPIFactory::GetForProfile(profile);
}

}  // namespace file_manager
