// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback.h"
#include "base/debug/leak_annotations.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "components/safe_browsing_db/v4_database.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace safe_browsing {

// static
V4StoreFactory* V4Database::factory_ = NULL;

// static
void V4Database::Create(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    const base::FilePath& base_path,
    const ListInfoMap& list_info_map,
    NewDatabaseReadyCallback callback) {
  // Create the database, which may be a lengthy operation, on the
  // db_task_runner, but once that is done, call the caller back on this
  // thread.
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
    const ListInfoMap& list_info_map,
    const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner,
    NewDatabaseReadyCallback callback) {
  DCHECK(db_task_runner->RunsTasksOnCurrentThread());
  DCHECK(base_path.IsAbsolute());

  if (!factory_) {
    factory_ = new V4StoreFactory();
    ANNOTATE_LEAKING_OBJECT_PTR(factory_);
  }
  auto store_map = base::MakeUnique<StoreMap>();
  for (const auto& list_info : list_info_map) {
    const UpdateListIdentifier& update_list_identifier = list_info.first;
    const base::FilePath store_path = base_path.AppendASCII(list_info.second);
    (*store_map)[update_list_identifier].reset(
        factory_->CreateV4Store(db_task_runner, store_path));
  }
  std::unique_ptr<V4Database> v4_database(
      new V4Database(db_task_runner, std::move(store_map)));

  // Database is done loading, pass it to the callback on the caller's thread.
  callback_task_runner->PostTask(
      FROM_HERE, base::Bind(callback, base::Passed(&v4_database)));
}

V4Database::V4Database(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    std::unique_ptr<StoreMap> store_map)
    : db_task_runner_(db_task_runner), store_map_(std::move(store_map)) {
  DCHECK(db_task_runner->RunsTasksOnCurrentThread());
  // TODO(vakh): Implement skeleton
}

// static
void V4Database::Destroy(std::unique_ptr<V4Database> v4_database) {
  V4Database* v4_database_raw = v4_database.release();
  if (v4_database_raw) {
    v4_database_raw->db_task_runner_->DeleteSoon(FROM_HERE, v4_database_raw);
  }
}

V4Database::~V4Database() {
  DCHECK(db_task_runner_->RunsTasksOnCurrentThread());
}

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
