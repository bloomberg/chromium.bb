// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/sync_manager.h"

namespace syncer {

SyncCredentials::SyncCredentials() {}

SyncCredentials::SyncCredentials(const SyncCredentials& other) = default;

SyncCredentials::~SyncCredentials() {}

SyncManager::ChangeDelegate::~ChangeDelegate() {}

SyncManager::ChangeObserver::~ChangeObserver() {}

SyncManager::Observer::~Observer() {}

SyncManager::InitArgs::InitArgs()
    : extensions_activity(nullptr),
      change_delegate(nullptr),
      encryptor(nullptr),
      cancelation_signal(nullptr) {}

SyncManager::InitArgs::~InitArgs() {}

SyncManager::SyncManager() {}

SyncManager::~SyncManager() {}

}  // namespace syncer
