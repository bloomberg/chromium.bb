// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/waitable_event.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
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
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"
#include "webkit/glue/form_field.h"

using base::Time;
using base::TimeDelta;
using base::WaitableEvent;
using testing::_;
using testing::DoDefault;
using testing::ElementsAreArray;
using testing::Pointee;
using testing::Property;

typedef std::vector<AutofillChange> AutofillChangeList;

static const int kWebDataServiceTimeoutSeconds = 8;

ACTION_P(SignalEvent, event) {
  event->Signal();
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

  WebDataService::Handle handle() { return handle_; }
  const std::vector<string16>& values() { return values_; }

 private:
  WebDataService::Handle handle_;
  std::vector<string16> values_;
  DISALLOW_COPY_AND_ASSIGN(AutofillWebDataServiceConsumer);
};

// This class will add and remove a mock notification observer from
// the DB thread.
class DBThreadObserverHelper :
    public base::RefCountedThreadSafe<DBThreadObserverHelper,
                                      ChromeThread::DeleteOnDBThread> {
 public:
  DBThreadObserverHelper() : done_event_(true, false) {}

  void Init() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
    ChromeThread::PostTask(
        ChromeThread::DB,
        FROM_HERE,
        NewRunnableMethod(this, &DBThreadObserverHelper::AddObserverTask));
    done_event_.Wait();
  }

  virtual ~DBThreadObserverHelper() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
    registrar_.RemoveAll();
  }

  NotificationObserverMock* observer() {
    return &observer_;
  }

 private:
  friend class base::RefCountedThreadSafe<DBThreadObserverHelper>;

  void AddObserverTask() {
    registrar_.Add(&observer_,
                   NotificationType::AUTOFILL_ENTRIES_CHANGED,
                   NotificationService::AllSources());
    registrar_.Add(&observer_,
                   NotificationType::AUTOFILL_PROFILE_CHANGED,
                   NotificationService::AllSources());
    registrar_.Add(&observer_,
                   NotificationType::AUTOFILL_CREDIT_CARD_CHANGED,
                   NotificationService::AllSources());
    done_event_.Signal();
  }

  WaitableEvent done_event_;
  NotificationRegistrar registrar_;
  NotificationObserverMock observer_;
};

class WebDataServiceTest : public testing::Test {
 public:
  WebDataServiceTest()
      : ui_thread_(ChromeThread::UI, &message_loop_),
        db_thread_(ChromeThread::DB) {}

 protected:
  virtual void SetUp() {
    db_thread_.Start();

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

    db_thread_.Stop();
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
    MessageLoop::current()->Run();
  }

  MessageLoopForUI message_loop_;
  ChromeThread ui_thread_;
  ChromeThread db_thread_;
  FilePath profile_dir_;
  scoped_refptr<WebDataService> wds_;
};

class WebDataServiceAutofillTest : public WebDataServiceTest {
 public:
  WebDataServiceAutofillTest()
      : WebDataServiceTest(),
        unique_id1_(1),
        unique_id2_(2),
        test_timeout_(TimeDelta::FromSeconds(kWebDataServiceTimeoutSeconds)),
        done_event_(false, false) {}

 protected:
  virtual void SetUp() {
    WebDataServiceTest::SetUp();
    name1_ = ASCIIToUTF16("name1");
    name2_ = ASCIIToUTF16("name2");
    value1_ = ASCIIToUTF16("value1");
    value2_ = ASCIIToUTF16("value2");
    observer_helper_ = new DBThreadObserverHelper();
    observer_helper_->Init();
  }

  virtual void TearDown() {
    // Release this first so it can get destructed on the db thread.
    observer_helper_ = NULL;
    WebDataServiceTest::TearDown();
  }

  void AppendFormField(const string16& name,
                       const string16& value,
                       std::vector<webkit_glue::FormField>* form_fields) {
    form_fields->push_back(
        webkit_glue::FormField(string16(),
                               name,
                               value,
                               string16()));
  }

  string16 name1_;
  string16 name2_;
  string16 value1_;
  string16 value2_;
  int unique_id1_, unique_id2_;
  const TimeDelta test_timeout_;
  scoped_refptr<DBThreadObserverHelper> observer_helper_;
  WaitableEvent done_event_;
};

TEST_F(WebDataServiceAutofillTest, FormFillAdd) {
  const AutofillChange expected_changes[] = {
    AutofillChange(AutofillChange::ADD, AutofillKey(name1_, value1_)),
    AutofillChange(AutofillChange::ADD, AutofillKey(name2_, value2_))
  };

  // This will verify that the correct notification is triggered,
  // passing the correct list of autofill keys in the details.
  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(NotificationType(NotificationType::AUTOFILL_ENTRIES_CHANGED),
              NotificationService::AllSources(),
              Property(&Details<const AutofillChangeList>::ptr,
                       Pointee(ElementsAreArray(expected_changes))))).
      WillOnce(SignalEvent(&done_event_));

  std::vector<webkit_glue::FormField> form_fields;
  AppendFormField(name1_, value1_, &form_fields);
  AppendFormField(name2_, value2_, &form_fields);
  wds_->AddFormFieldValues(form_fields);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);

  AutofillWebDataServiceConsumer consumer;
  WebDataService::Handle handle;
  static const int limit = 10;
  handle = wds_->GetFormValuesForElementName(
      name1_, string16(), limit, &consumer);

  // The message loop will exit when the consumer is called.
  MessageLoop::current()->Run();

  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(1U, consumer.values().size());
  EXPECT_EQ(value1_, consumer.values()[0]);
}

TEST_F(WebDataServiceAutofillTest, FormFillRemoveOne) {
  // First add some values to autofill.
  EXPECT_CALL(*observer_helper_->observer(), Observe(_, _, _)).
      WillOnce(SignalEvent(&done_event_));
  std::vector<webkit_glue::FormField> form_fields;
  AppendFormField(name1_, value1_, &form_fields);
  wds_->AddFormFieldValues(form_fields);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);

  // This will verify that the correct notification is triggered,
  // passing the correct list of autofill keys in the details.
  const AutofillChange expected_changes[] = {
    AutofillChange(AutofillChange::REMOVE, AutofillKey(name1_, value1_))
  };
  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(NotificationType(NotificationType::AUTOFILL_ENTRIES_CHANGED),
              NotificationService::AllSources(),
              Property(&Details<const AutofillChangeList>::ptr,
                       Pointee(ElementsAreArray(expected_changes))))).
      WillOnce(SignalEvent(&done_event_));
  wds_->RemoveFormValueForElementName(name1_, value1_);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);
}

TEST_F(WebDataServiceAutofillTest,FormFillRemoveMany) {
  TimeDelta one_day(TimeDelta::FromDays(1));
  Time t = Time::Now();

  EXPECT_CALL(*observer_helper_->observer(), Observe(_, _, _)).
      WillOnce(SignalEvent(&done_event_));
  std::vector<webkit_glue::FormField> form_fields;
  AppendFormField(name1_, value1_, &form_fields);
  AppendFormField(name2_, value2_, &form_fields);
  wds_->AddFormFieldValues(form_fields);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);

  // This will verify that the correct notification is triggered,
  // passing the correct list of autofill keys in the details.
  const AutofillChange expected_changes[] = {
    AutofillChange(AutofillChange::REMOVE, AutofillKey(name1_, value1_)),
    AutofillChange(AutofillChange::REMOVE, AutofillKey(name2_, value2_))
  };
  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(NotificationType(NotificationType::AUTOFILL_ENTRIES_CHANGED),
              NotificationService::AllSources(),
              Property(&Details<const AutofillChangeList>::ptr,
                       Pointee(ElementsAreArray(expected_changes))))).
      WillOnce(SignalEvent(&done_event_));
  wds_->RemoveFormElementsAddedBetween(t, t + one_day);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);
}

TEST_F(WebDataServiceAutofillTest, ProfileAdd) {
  AutoFillProfile profile(name1_, unique_id1_);
  const AutofillProfileChange expected_change(
      AutofillProfileChange::ADD, unique_id1_, &profile);

  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(NotificationType(NotificationType::AUTOFILL_PROFILE_CHANGED),
              NotificationService::AllSources(),
              Property(&Details<const AutofillProfileChange>::ptr,
                       Pointee(expected_change)))).
      WillOnce(SignalEvent(&done_event_));

  wds_->AddAutoFillProfile(profile);
  done_event_.TimedWait(test_timeout_);
}

TEST_F(WebDataServiceAutofillTest, ProfileRemove) {
  AutoFillProfile profile(name1_, unique_id1_);

  EXPECT_CALL(*observer_helper_->observer(), Observe(_, _, _)).
      WillOnce(SignalEvent(&done_event_));
  wds_->AddAutoFillProfile(profile);
  done_event_.TimedWait(test_timeout_);

  const AutofillProfileChange expected_change(
      AutofillProfileChange::REMOVE, unique_id1_, NULL);
  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(NotificationType(NotificationType::AUTOFILL_PROFILE_CHANGED),
              NotificationService::AllSources(),
              Property(&Details<const AutofillProfileChange>::ptr,
                       Pointee(expected_change)))).
      WillOnce(SignalEvent(&done_event_));

  wds_->RemoveAutoFillProfile(profile.unique_id());
  done_event_.TimedWait(test_timeout_);
}

TEST_F(WebDataServiceAutofillTest, ProfileUpdate) {
  AutoFillProfile profile1(name1_, unique_id1_);
  AutoFillProfile profile2(name2_, unique_id2_);

  EXPECT_CALL(*observer_helper_->observer(), Observe(_, _, _)).
      Times(2).
      WillOnce(DoDefault()).
      WillOnce(SignalEvent(&done_event_));
  wds_->AddAutoFillProfile(profile1);
  wds_->AddAutoFillProfile(profile2);

  done_event_.TimedWait(test_timeout_);

  AutoFillProfile profile1_delta(profile1);
  profile1_delta.set_label(ASCIIToUTF16("new_label!"));
  const AutofillProfileChange expected_change(
      AutofillProfileChange::UPDATE, unique_id1_, &profile1_delta);

  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(NotificationType(NotificationType::AUTOFILL_PROFILE_CHANGED),
              NotificationService::AllSources(),
              Property(&Details<const AutofillProfileChange>::ptr,
                       Pointee(expected_change)))).
      WillOnce(SignalEvent(&done_event_));

  wds_->UpdateAutoFillProfile(profile1_delta);
  done_event_.TimedWait(test_timeout_);
}

TEST_F(WebDataServiceAutofillTest, CreditAdd) {
  CreditCard card(name1_, unique_id1_);
  const AutofillCreditCardChange expected_change(
      AutofillCreditCardChange::ADD, unique_id1_, &card);

  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(NotificationType(NotificationType::AUTOFILL_CREDIT_CARD_CHANGED),
              NotificationService::AllSources(),
              Property(&Details<const AutofillCreditCardChange>::ptr,
                       Pointee(expected_change)))).
      WillOnce(SignalEvent(&done_event_));

  wds_->AddCreditCard(card);
  done_event_.TimedWait(test_timeout_);
}

TEST_F(WebDataServiceAutofillTest, CreditRemove) {
  CreditCard card(name1_, unique_id1_);
  EXPECT_CALL(*observer_helper_->observer(), Observe(_, _, _)).
      WillOnce(SignalEvent(&done_event_));
  wds_->AddCreditCard(card);
  done_event_.TimedWait(test_timeout_);

  const AutofillCreditCardChange expected_change(
      AutofillCreditCardChange::REMOVE, unique_id1_, NULL);

  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(NotificationType(NotificationType::AUTOFILL_CREDIT_CARD_CHANGED),
              NotificationService::AllSources(),
              Property(&Details<const AutofillCreditCardChange>::ptr,
                       Pointee(expected_change)))).
      WillOnce(SignalEvent(&done_event_));

  wds_->RemoveCreditCard(card.unique_id());
  done_event_.TimedWait(test_timeout_);
}

TEST_F(WebDataServiceAutofillTest, CreditUpdate) {
  CreditCard card1(name1_, unique_id1_);
  CreditCard card2(name2_, unique_id2_);

  EXPECT_CALL(*observer_helper_->observer(), Observe(_, _, _)).
      Times(2).
      WillOnce(DoDefault()).
      WillOnce(SignalEvent(&done_event_));
  wds_->AddCreditCard(card1);
  wds_->AddCreditCard(card2);
  done_event_.TimedWait(test_timeout_);

  CreditCard card1_delta(card1);
  card1_delta.set_label(ASCIIToUTF16("new_label!"));
  const AutofillCreditCardChange expected_change(
      AutofillCreditCardChange::UPDATE, unique_id1_, &card1_delta);

  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(NotificationType(NotificationType::AUTOFILL_CREDIT_CARD_CHANGED),
              NotificationService::AllSources(),
              Property(&Details<const AutofillCreditCardChange>::ptr,
                       Pointee(expected_change)))).
      WillOnce(SignalEvent(&done_event_));

  wds_->UpdateCreditCard(card1_delta);
  done_event_.TimedWait(test_timeout_);
}

