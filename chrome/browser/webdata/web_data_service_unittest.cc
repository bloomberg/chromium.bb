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
#include "base/time.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/webdata/autofill_change.h"
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

using base::Time;
using base::TimeDelta;
using testing::_;
using testing::ElementsAreArray;
using testing::Pointee;
using testing::Property;

typedef std::vector<AutofillChange> AutofillChangeList;

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
  WebDataServiceTest()
      : ui_thread_(ChromeThread::UI, &message_loop_) {}

 protected:
  virtual void SetUp() {
    name1_ = ASCIIToUTF16("name1");
    name2_ = ASCIIToUTF16("name2");
    value1_ = ASCIIToUTF16("value1");
    value2_ = ASCIIToUTF16("value2");

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

  void AppendFormField(const string16& name,
                       const string16& value,
                       std::vector<webkit_glue::FormField>* form_fields) {
    form_fields->push_back(
        webkit_glue::FormField(EmptyString16(),
                               name,
                               EmptyString16(),
                               value));
  }

  MessageLoopForUI message_loop_;
  ChromeThread ui_thread_;
  string16 name1_;
  string16 name2_;
  string16 value1_;
  string16 value2_;
  FilePath profile_dir_;
  scoped_refptr<WebDataService> wds_;
  NotificationRegistrar registrar_;
  NotificationObserverMock observer_;
};

TEST_F(WebDataServiceTest, AutofillAdd) {
  const AutofillChange expected_changes[] = {
    AutofillChange(AutofillChange::ADD, AutofillKey(name1_, value1_)),
    AutofillChange(AutofillChange::ADD, AutofillKey(name2_, value2_))
  };

  // This will verify that the correct notification is triggered,
  // passing the correct list of autofill keys in the details.
  EXPECT_CALL(
      observer_,
      Observe(NotificationType(NotificationType::AUTOFILL_ENTRIES_CHANGED),
              NotificationService::AllSources(),
              Property(&Details<const AutofillChangeList>::ptr,
                       Pointee(ElementsAreArray(expected_changes))))).
      WillOnce(QuitUIMessageLoop());

  registrar_.Add(&observer_,
                 NotificationType::AUTOFILL_ENTRIES_CHANGED,
                 NotificationService::AllSources());

  std::vector<webkit_glue::FormField> form_fields;
  AppendFormField(name1_, value1_, &form_fields);
  AppendFormField(name2_, value2_, &form_fields);
  wds_->AddFormFieldValues(form_fields);

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  AutofillWebDataServiceConsumer consumer;
  WebDataService::Handle handle;
  static const int limit = 10;
  handle = wds_->GetFormValuesForElementName(
      name1_, EmptyString16(), limit, &consumer);

  // The message loop will exit when the consumer is called.
  MessageLoop::current()->Run();

  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(1U, consumer.values().size());
  EXPECT_EQ(value1_, consumer.values()[0]);
}

TEST_F(WebDataServiceTest, AutofillRemoveOne) {
  // First add some values to autofill.
  EXPECT_CALL(observer_, Observe(_, _, _)).WillOnce(QuitUIMessageLoop());
  registrar_.Add(&observer_,
                 NotificationType::AUTOFILL_ENTRIES_CHANGED,
                 NotificationService::AllSources());
  std::vector<webkit_glue::FormField> form_fields;
  AppendFormField(name1_, value1_, &form_fields);
  wds_->AddFormFieldValues(form_fields);

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  // This will verify that the correct notification is triggered,
  // passing the correct list of autofill keys in the details.
  const AutofillChange expected_changes[] = {
    AutofillChange(AutofillChange::REMOVE, AutofillKey(name1_, value1_))
  };
  EXPECT_CALL(
      observer_,
      Observe(NotificationType(NotificationType::AUTOFILL_ENTRIES_CHANGED),
              NotificationService::AllSources(),
              Property(&Details<const AutofillChangeList>::ptr,
                       Pointee(ElementsAreArray(expected_changes))))).
      WillOnce(QuitUIMessageLoop());
  wds_->RemoveFormValueForElementName(name1_, value1_);

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();
}

TEST_F(WebDataServiceTest, AutofillRemoveMany) {
  TimeDelta one_day(TimeDelta::FromDays(1));
  Time t = Time::Now();

  EXPECT_CALL(observer_, Observe(_, _, _)).WillOnce(QuitUIMessageLoop());
  registrar_.Add(&observer_,
                 NotificationType::AUTOFILL_ENTRIES_CHANGED,
                 NotificationService::AllSources());
  std::vector<webkit_glue::FormField> form_fields;
  AppendFormField(name1_, value1_, &form_fields);
  AppendFormField(name2_, value2_, &form_fields);
  wds_->AddFormFieldValues(form_fields);

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  // This will verify that the correct notification is triggered,
  // passing the correct list of autofill keys in the details.
  const AutofillChange expected_changes[] = {
    AutofillChange(AutofillChange::REMOVE, AutofillKey(name1_, value1_)),
    AutofillChange(AutofillChange::REMOVE, AutofillKey(name2_, value2_))
  };
  EXPECT_CALL(
      observer_,
      Observe(NotificationType(NotificationType::AUTOFILL_ENTRIES_CHANGED),
              NotificationService::AllSources(),
              Property(&Details<const AutofillChangeList>::ptr,
                       Pointee(ElementsAreArray(expected_changes))))).
      WillOnce(QuitUIMessageLoop());
  wds_->RemoveFormElementsAddedBetween(t, t + one_day);

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();
}
