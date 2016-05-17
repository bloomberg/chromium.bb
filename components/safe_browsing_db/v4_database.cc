// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "components/safe_browsing_db/v4_database.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

V4Store* CreateStore(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::FilePath& store_path) {
  return new V4Store(task_runner, store_path);
}

}  // namespace

// static
V4DatabaseFactory* V4Database::factory_ = NULL;

// static
void V4Database::Create(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::FilePath& base_path,
    ListInfoMap list_info_map,
    NewDatabaseReadyCallback callback) {
  const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner =
      base::MessageLoop::current()->task_runner();
  db_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&V4Database::CreateOnTaskRunner, db_task_runner, base_path,
                 list_info_map, callback_task_runner, callback));
}

// static
void V4Database::CreateOnTaskRunner(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::FilePath& base_path,
    ListInfoMap list_info_map,
    const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner,
    NewDatabaseReadyCallback callback) {
  DCHECK(db_task_runner->RunsTasksOnCurrentThread());
  DCHECK(!base_path.empty());

  std::unique_ptr<V4Database> v4_database;
  if (!factory_) {
    StoreMap store_map;

    for (const auto& list_info : list_info_map) {
      UpdateListIdentifier update_list_identifier = list_info.first;
      const base::FilePath::CharType suffix = list_info.second;

      const base::FilePath store_path =
          base::FilePath(base_path.value() + suffix);
      (*store_map)[update_list_identifier].reset(
          CreateStore(db_task_runner, store_path));
    }

    v4_database.reset(new V4Database(db_task_runner, std::move(store_map)));
  } else {
    v4_database.reset(
        factory_->CreateV4Database(db_task_runner, base_path, list_info_map));
  }
  callback_task_runner->PostTask(
      FROM_HERE, base::Bind(callback, base::Passed(&v4_database)));
}

V4Database::V4Database(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    StoreMap store_map)
    : db_task_runner_(db_task_runner), store_map_(std::move(store_map)) {
  DCHECK(db_task_runner->RunsTasksOnCurrentThread());
  // TODO(vakh): Implement skeleton
}

// static
void V4Database::Destroy(std::unique_ptr<V4Database> v4_database) {
  if (v4_database.get()) {
    v4_database->db_task_runner_->DeleteSoon(FROM_HERE, v4_database.release());
  }
}

V4Database::~V4Database() {}

bool V4Database::ResetDatabase() {
  DCHECK(db_task_runner_->RunsTasksOnCurrentThread());
  bool reset_success = true;
  for (const auto& store_id_and_store : *store_map_) {
    if (!store_id_and_store.second->Reset()) {
      reset_success = false;
    }
  }
  return reset_success;
}

}  // namespace safe_browsing
