// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/v4_database.h"

namespace safe_browsing {

// static
V4DatabaseFactory* V4Database::factory_ = NULL;

// static
// Factory method, should be called on the Safe Browsing sequenced task runner,
// which is also passed to the function as |db_task_runner|.
V4Database* V4Database::Create(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::FilePath& base_path,
    ListInfoMap list_info_map) {
  DCHECK(db_task_runner->RunsTasksOnCurrentThread());
  if (!factory_) {
    StoreMap store_map;
    // TODO(vakh): Populate the store_map using list_suffix_map.
    return new V4Database(db_task_runner, std::move(store_map));
  } else {
    return factory_->CreateV4Database(db_task_runner, base_path, list_info_map);
  }
}

V4Database::V4Database(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    StoreMap store_map) {
  // TODO(vakh): Implement skeleton
}

V4Database::~V4Database() {}

bool V4Database::ResetDatabase() {
  // TODO(vakh): Delete the stores. Delete the backing files.
  return true;
}

}  // namespace safe_browsing
