// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/ios/reading_list_model_storage.h"

ReadingListModelStorage::ReadingListModelStorage(
    const ChangeProcessorFactory& change_processor_factory,
    syncer::ModelType type)
    : ModelTypeSyncBridge(change_processor_factory, type) {}

ReadingListModelStorage::~ReadingListModelStorage() {}
