// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/form_field.h"

class AutofillWebDataServiceConsumer: public WebDataServiceConsumer {
 public:
  AutofillWebDataServiceConsumer() {}
  virtual ~AutofillWebDataServiceConsumer() {}

  virtual void OnWebDataServiceRequestDone(WebDataService::Handle handle,
                                           const WDTypedResult* result) {
    DCHECK(result->GetType() == AUTOFILL_VALUE_RESULT);
    handle_ = handle;
    const WDResult<std::vector<string16> >* autofill_result =
        static_cast<const WDResult<std::vector<string16> >*>(result);
    // Copy the values.
    values_ = autofill_result->GetValue();
    MessageLoop::current()->Quit();
  }

  WebDataService::Handle const handle() { return handle_; }
  const std::vector<string16>& values() { return values_; }

 private:
  WebDataService::Handle handle_;
  std::vector<string16> values_;
  DISALLOW_COPY_AND_ASSIGN(AutofillWebDataServiceConsumer);
};

class WebDataServiceTest : public testing::Test {
 protected:

  virtual void SetUp() {
    PathService::Get(chrome::DIR_TEST_DATA, &profile_dir_);
    const std::string test_profile = "WebDataServiceTest";
    profile_dir_ = profile_dir_.AppendASCII(test_profile);
    file_util::Delete(profile_dir_, true);
    file_util::CreateDirectory(profile_dir_);
    wds_ = new WebDataService();
    wds_->Init(profile_dir_);
  }

  virtual void TearDown() {
    if (wds_.get())
      wds_->Shutdown();
    file_util::Delete(profile_dir_, true);

    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
    MessageLoop::current()->Run();
  }

  MessageLoopForUI message_loop_;
  FilePath profile_dir_;
  scoped_refptr<WebDataService> wds_;
};

TEST_F(WebDataServiceTest, AutofillAdd) {
  std::vector<webkit_glue::FormField> form_fields;
  form_fields.push_back(webkit_glue::FormField(EmptyString16(),
                                               ASCIIToUTF16("name"),
                                               EmptyString16(),
                                               ASCIIToUTF16("value")));
  wds_->AddFormFieldValues(form_fields);

  AutofillWebDataServiceConsumer consumer;
  WebDataService::Handle handle;
  static const int limit = 10;
  handle = wds_->GetFormValuesForElementName(ASCIIToUTF16("name"),
                                             EmptyString16(),
                                             limit,
                                             &consumer);
  MessageLoop::current()->Run();

  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(1U, consumer.values().size());
  EXPECT_EQ(ASCIIToUTF16("value"), consumer.values()[0]);
}
