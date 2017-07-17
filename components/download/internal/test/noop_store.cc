// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/test/noop_store.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/internal/entry.h"

namespace download {

NoopStore::NoopStore() : initialized_(false), weak_ptr_factory_(this) {}

NoopStore::~NoopStore() = default;

bool NoopStore::IsInitialized() {
  return initialized_;
}

void NoopStore::Initialize(InitCallback callback) {
  DCHECK(!IsInitialized());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&NoopStore::OnInitFinished, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void NoopStore::HardRecover(StoreCallback callback) {
  initialized_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void NoopStore::Update(const Entry& entry, StoreCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true /** success */));
}

void NoopStore::Remove(const std::string& guid, StoreCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true /** success */));
}

void NoopStore::OnInitFinished(InitCallback callback) {
  initialized_ = true;

  std::unique_ptr<std::vector<Entry>> entries =
      base::MakeUnique<std::vector<Entry>>();
  std::move(callback).Run(true /** success */, std::move(entries));
}

}  // namespace download
