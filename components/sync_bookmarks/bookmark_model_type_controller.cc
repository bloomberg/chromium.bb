// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_type_controller.h"

namespace sync_bookmarks {

BookmarkModelTypeController::BookmarkModelTypeController()
    : DataTypeController(syncer::BOOKMARKS) {}

bool BookmarkModelTypeController::ShouldLoadModelBeforeConfigure() const {
  NOTIMPLEMENTED();
  return false;
}

void BookmarkModelTypeController::BeforeLoadModels(
    syncer::ModelTypeConfigurer* configurer) {
  NOTIMPLEMENTED();
}

void BookmarkModelTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  NOTIMPLEMENTED();
}

void BookmarkModelTypeController::RegisterWithBackend(
    base::Callback<void(bool)> set_downloaded,
    syncer::ModelTypeConfigurer* configurer) {
  NOTIMPLEMENTED();
}

void BookmarkModelTypeController::StartAssociating(
    const StartCallback& start_callback) {
  NOTIMPLEMENTED();
}

void BookmarkModelTypeController::ActivateDataType(
    syncer::ModelTypeConfigurer* configurer) {
  NOTIMPLEMENTED();
}

void BookmarkModelTypeController::DeactivateDataType(
    syncer::ModelTypeConfigurer* configurer) {
  NOTIMPLEMENTED();
}

void BookmarkModelTypeController::Stop() {
  NOTIMPLEMENTED();
}

syncer::DataTypeController::State BookmarkModelTypeController::state() const {
  NOTIMPLEMENTED();
  return NOT_RUNNING;
}

void BookmarkModelTypeController::GetAllNodes(
    const AllNodesCallback& callback) {
  NOTIMPLEMENTED();
}

void BookmarkModelTypeController::GetStatusCounters(
    const StatusCountersCallback& callback) {
  NOTIMPLEMENTED();
}

void BookmarkModelTypeController::RecordMemoryUsageHistogram() {
  NOTIMPLEMENTED();
}

}  // namespace sync_bookmarks
