// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/update_client/task_update.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/update_client/update_engine.h"

namespace update_client {

TaskUpdate::TaskUpdate(UpdateEngine* update_engine,
                       const std::vector<std::string>& ids,
                       const UpdateClient::CrxDataCallback& crx_data_callback)
    : update_engine_(update_engine),
      ids_(ids),
      crx_data_callback_(crx_data_callback) {
}

TaskUpdate::~TaskUpdate() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void TaskUpdate::Run(const Callback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  callback_ = callback;

  if (ids_.empty())
    RunComplete(-1);

  update_engine_->Update(
      ids_, crx_data_callback_,
      base::Bind(&TaskUpdate::RunComplete, base::Unretained(this)));
}

void TaskUpdate::RunComplete(int error) {
  DCHECK(thread_checker_.CalledOnValidThread());

  callback_.Run(this, error);
}

}  // namespace update_client
