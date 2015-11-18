// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_tab_model_test_utils.h"

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/android/data_usage/data_use_tab_model.h"
#include "chrome/browser/android/data_usage/external_data_use_observer.h"
#include "components/data_usage/core/data_use.h"
#include "url/gurl.h"

namespace chrome {

namespace android {

TestDataUseTabModel::TestDataUseTabModel(
    const ExternalDataUseObserver* data_use_observer,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : DataUseTabModel(data_use_observer, ui_task_runner.get()) {}

TestDataUseTabModel::~TestDataUseTabModel() {}

bool TestDataUseTabModel::GetLabelForDataUse(
    const data_usage::DataUse& data_use,
    std::string* output_label) const {
  return data_use_observer_->Matches(data_use.url, output_label);
}

}  // namespace android

}  // namespace chrome
