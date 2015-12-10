// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_TAB_MODEL_TEST_UTILS_H_
#define CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_TAB_MODEL_TEST_UTILS_H_

#include "base/memory/ref_counted.h"
#include "chrome/browser/android/data_usage/data_use_tab_model.h"
#include "components/data_usage/core/data_use.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace chrome {

namespace android {

class TestDataUseTabModel : public DataUseTabModel {
 public:
  explicit TestDataUseTabModel(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner);

  ~TestDataUseTabModel() override;

  bool GetLabelForDataUse(const data_usage::DataUse& data_use,
                          std::string* output_label) const override;

  MOCK_METHOD1(OnTrackingLabelRemoved, void(std::string label));
};

}  // namespace android

}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_DATA_USAGE_DATA_USE_TAB_MODEL_TEST_UTILS_H_
