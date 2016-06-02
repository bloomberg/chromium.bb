// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/v4_store.h"

namespace safe_browsing {

V4Store* V4StoreFactory::CreateV4Store(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::FilePath& store_path) {
  return new V4Store(task_runner, store_path);
}

V4Store::V4Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                 const base::FilePath& store_path)
    : task_runner_(task_runner), store_path_(store_path) {}

V4Store::~V4Store() {}

bool V4Store::Reset() {
  // TODO(vakh): Implement skeleton.
  return true;
}

}  // namespace safe_browsing
