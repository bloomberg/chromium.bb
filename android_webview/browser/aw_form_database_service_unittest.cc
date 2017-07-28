// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_form_database_service.h"

#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/form_field_data.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util_android.h"

using autofill::AutofillWebDataService;
using autofill::FormFieldData;
using base::android::AttachCurrentThread;
using testing::Test;

namespace android_webview {

class AwFormDatabaseServiceTest : public Test {
 public:
  AwFormDatabaseServiceTest() {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    env_ = AttachCurrentThread();
    ASSERT_TRUE(env_ != NULL);
    ASSERT_TRUE(l10n_util::RegisterLocalizationUtil(env_));

    service_.reset(new AwFormDatabaseService(temp_dir_.GetPath()));
  }

  void TearDown() override {
    service_->Shutdown();
    content::RunAllBlockingPoolTasksUntilIdle();
  }

  // The path to the temporary directory used for the test operations.
  base::ScopedTempDir temp_dir_;
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  JNIEnv* env_;
  std::unique_ptr<AwFormDatabaseService> service_;
};

TEST_F(AwFormDatabaseServiceTest, HasAndClearFormData) {
  EXPECT_FALSE(service_->HasFormData());
  std::vector<FormFieldData> fields;
  FormFieldData field;
  field.name = base::ASCIIToUTF16("foo");
  field.value = base::ASCIIToUTF16("bar");
  fields.push_back(field);
  service_->get_autofill_webdata_service()->AddFormFields(fields);
  EXPECT_TRUE(service_->HasFormData());
  service_->ClearFormData();
  EXPECT_FALSE(service_->HasFormData());
}

}  // namespace android_webview
