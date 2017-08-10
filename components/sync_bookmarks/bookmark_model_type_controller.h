// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_CONTROLLER_H_

#include "base/macros.h"
#include "components/sync/driver/data_type_controller.h"

namespace sync_bookmarks {

// A class that manages the startup and shutdown of bookmark sync implemented
// through USS APIs.
class BookmarkModelTypeController : public syncer::DataTypeController {
 public:
  BookmarkModelTypeController();

  // syncer::DataTypeController implementation.
  bool ShouldLoadModelBeforeConfigure() const override;
  void BeforeLoadModels(syncer::ModelTypeConfigurer* configurer) override;
  void LoadModels(const ModelLoadCallback& model_load_callback) override;
  void RegisterWithBackend(base::Callback<void(bool)> set_downloaded,
                           syncer::ModelTypeConfigurer* configurer) override;
  void StartAssociating(const StartCallback& start_callback) override;
  void ActivateDataType(syncer::ModelTypeConfigurer* configurer) override;
  void DeactivateDataType(syncer::ModelTypeConfigurer* configurer) override;
  void Stop() override;
  State state() const override;
  void GetAllNodes(const AllNodesCallback& callback) override;
  void GetStatusCounters(const StatusCountersCallback& callback) override;
  void RecordMemoryUsageHistogram() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkModelTypeController);
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_CONTROLLER_H_
