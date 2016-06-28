// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "components/safe_browsing_db/v4_store.h"

namespace safe_browsing {

std::ostream& operator<<(std::ostream& os, const V4Store& store) {
  os << store.DebugString();
  return os;
}

V4Store* V4StoreFactory::CreateV4Store(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::FilePath& store_path) {
  return new V4Store(task_runner, store_path);
}

V4Store::V4Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                 const base::FilePath& store_path)
    : store_path_(store_path), task_runner_(task_runner) {}

V4Store::~V4Store() {}

std::string V4Store::DebugString() const {
  std::string state_base64;
  base::Base64Encode(state_, &state_base64);

  return base::StringPrintf("path: %s; state: %s", store_path_.value().c_str(),
                            state_base64.c_str());
}

bool V4Store::Reset() {
  // TODO(vakh): Implement skeleton.
  state_ = "";
  return true;
}

void V4Store::ApplyUpdate(
    const ListUpdateResponse& response,
    const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner,
    UpdatedStoreReadyCallback callback) {
  std::unique_ptr<V4Store> new_store(
      new V4Store(this->task_runner_, this->store_path_));

  // TODO(vakh): The new store currently only picks up the new state. Do more.
  new_store->state_ = response.new_client_state();

  // new_store is done updating, pass it to the callback.
  callback_task_runner->PostTask(
      FROM_HERE, base::Bind(callback, base::Passed(&new_store)));
}

}  // namespace safe_browsing
