// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/previews_ui_service.h"

#include "components/previews/previews_io_data.h"

namespace previews {

PreviewsUIService::PreviewsUIService(
    PreviewsIOData* previews_io_data,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : io_task_runner_(io_task_runner), weak_factory_(this) {
  previews_io_data->Initialize(weak_factory_.GetWeakPtr());
}

PreviewsUIService::~PreviewsUIService() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void PreviewsUIService::SetIOData(base::WeakPtr<PreviewsIOData> io_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  io_data_ = io_data;
}

}  // namespace previews
