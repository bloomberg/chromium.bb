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
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer_mock.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/form_field.h"

using testing::ElementsAreArray;
using testing::Pointee;
using testing::Property;

typedef std::vector<AutofillKey> AutofillKeyList;

ACTION(QuitUIMessageLoop) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  MessageLoop::current()->Quit();
}

class AutofillWebDataServiceConsumer: public WebDataServiceConsumer {
 public:
  AutofillWebDataServiceConsumer() : handle_(0) {}
  virtual ~AutofillWebDataServiceConsumer() {}

  virtual void OnWebDataServiceRequestDone(WebDataService::Handle handle,
                                           const WDTypedResult* result) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    DCHECK(result->GetType() == AUTOFILL_VALUE_RESULT);
    handle_ = handle;
    const WDResult<std::vector<string16> >* autofill_result =
        static_cast<const WDResult<std::vector<string16> >*>(result);
    // Copy the values.
    values_ = autofill_result->GetValue();

    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
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
 public:
  WebDataServiceTest() : ui_thread_(ChromeThread::UI, &message_loop_) {}
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
  ChromeThread ui_thread_;
  FilePath profile_dir_;
  scoped_refptr<WebDataService> wds_;
  NotificationRegistrar registrar_;
  NotificationObserverMock observer_;
};

TEST_F(WebDataServiceTest, AutofillAdd) {
  const string16 kName1 = ASCIIToUTF16("name1");
  const string16 kValue1 = ASCIIToUTF16("value1");
  const string16 kName2 = ASCIIToUTF16("name2");
  const string16 kValue2 = ASCIIToUTF16("value2");

  const AutofillKey expected_keys[] = {
    AutofillKey(kName1, kValue1),
    AutofillKey(kName2, kValue2)
  };

  // This will verify that the correct notification is triggered,
  // passing the correct list of autofill keys in the details.
  EXPECT_CALL(
      observer_,
      Observe(NotificationType(NotificationType::AUTOFILL_ENTRIES_ADDED),
              NotificationService::AllSources(),
              Property(&Details<AutofillKeyList>::ptr,
                       Pointee(ElementsAreArray(expected_keys))))).
      WillOnce(QuitUIMessageLoop());

  registrar_.Add(&observer_,
                 NotificationType::AUTOFILL_ENTRIES_ADDED,
                 NotificationService::AllSources());

  std::vector<webkit_glue::FormField> form_fields;
  form_fields.push_back(
      webkit_glue::FormField(EmptyString16(),
                             kName1,
                             EmptyString16(),
                             kValue1));
  form_fields.push_back(
      webkit_glue::FormField(EmptyString16(),
                             kName2,
                             EmptyString16(),
                             kValue2));
  wds_->AddFormFieldValues(form_fields);

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  AutofillWebDataServiceConsumer consumer;
  WebDataService::Handle handle;
  static const int limit = 10;
  handle = wds_->GetFormValuesForElementName(
      kName1, EmptyString16(), limit, &consumer);

  // The message loop will exit when the consumer is called.
  MessageLoop::current()->Run();

  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(1U, consumer.values().size());
  EXPECT_EQ(kValue1, consumer.values()[0]);
}
