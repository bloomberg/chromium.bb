// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/previews_io_data.h"

#include "base/bind.h"
#include "base/location.h"
#include "components/previews/previews_ui_service.h"

namespace previews {

PreviewsIOData::PreviewsIOData(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : ui_task_runner_(ui_task_runner),
      io_task_runner_(io_task_runner),
      weak_factory_(this) {}

PreviewsIOData::~PreviewsIOData() {}

void PreviewsIOData::Initialize(
    base::WeakPtr<PreviewsUIService> previews_ui_service) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  previews_ui_service_ = previews_ui_service;

  // Set up the IO thread portion of |this|.
  io_task_runner_->PostTask(FROM_HERE,
                            base::Bind(&PreviewsIOData::InitializeOnIOThread,
                                       base::Unretained(this)));
}

void PreviewsIOData::InitializeOnIOThread() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PreviewsUIService::SetIOData, previews_ui_service_,
                            weak_factory_.GetWeakPtr()));
}

}  // namespace previews
