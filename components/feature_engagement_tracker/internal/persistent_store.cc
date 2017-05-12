// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/persistent_store.h"

#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"

namespace feature_engagement_tracker {
namespace {
// Corresponds to a UMA suffix "LevelDBOpenResults" in histograms.xml.
// Please do not change.
const char kDatabaseUMAName[] = "FeatureEngagementTrackerStore";

using KeyEventPair = std::pair<std::string, Event>;
using KeyEventList = std::vector<KeyEventPair>;

void NoopUpdateCallback(bool success) {}
}  // namespace

PersistentStore::PersistentStore(
    const base::FilePath& storage_dir,
    std::unique_ptr<leveldb_proto::ProtoDatabase<Event>> db)
    : storage_dir_(storage_dir),
      db_(std::move(db)),
      ready_(false),
      weak_ptr_factory_(this) {}

PersistentStore::~PersistentStore() = default;

void PersistentStore::Load(const OnLoadedCallback& callback) {
  DCHECK(!ready_);

  db_->Init(kDatabaseUMAName, storage_dir_,
            base::Bind(&PersistentStore::OnInitComplete,
                       weak_ptr_factory_.GetWeakPtr(), callback));
}

bool PersistentStore::IsReady() const {
  return ready_;
}

void PersistentStore::WriteEvent(const Event& event) {
  DCHECK(IsReady());
  std::unique_ptr<KeyEventList> entries = base::MakeUnique<KeyEventList>();
  entries->push_back(KeyEventPair(event.name(), event));

  // TODO(dtrainor, nyquist): Consider tracking failures here and storing UMA.
  db_->UpdateEntries(std::move(entries),
                     base::MakeUnique<std::vector<std::string>>(),
                     base::Bind(&NoopUpdateCallback));
}

void PersistentStore::OnInitComplete(const OnLoadedCallback& callback,
                                     bool success) {
  if (!success) {
    callback.Run(false, base::MakeUnique<std::vector<Event>>());
    return;
  }

  db_->LoadEntries(base::Bind(&PersistentStore::OnLoadComplete,
                              weak_ptr_factory_.GetWeakPtr(), callback));
}

void PersistentStore::OnLoadComplete(
    const OnLoadedCallback& callback,
    bool success,
    std::unique_ptr<std::vector<Event>> entries) {
  ready_ = success;
  callback.Run(success, std::move(entries));
}

}  // namespace feature_engagement_tracker
