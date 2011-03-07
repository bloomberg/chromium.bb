// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "base/stl_util-inl.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/webdata/web_data_service_test_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/guid.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/thread_observer_helper.h"
#include "content/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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

class AutofillDBThreadObserverHelper : public DBThreadObserverHelper {
 protected:
  virtual void RegisterObservers() {
    registrar_.Add(&observer_,
                   NotificationType::AUTOFILL_ENTRIES_CHANGED,
                   NotificationService::AllSources());
    registrar_.Add(&observer_,
                   NotificationType::AUTOFILL_PROFILE_CHANGED,
                   NotificationService::AllSources());
    registrar_.Add(&observer_,
                   NotificationType::AUTOFILL_CREDIT_CARD_CHANGED,
                   NotificationService::AllSources());
  }
};

class WebDataServiceTest : public testing::Test {
 public:
  WebDataServiceTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB) {}

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
  BrowserThread ui_thread_;
  BrowserThread db_thread_;
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
    observer_helper_ = new AutofillDBThreadObserverHelper();
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
                               string16(),
                               0,
                               false));
  }

  string16 name1_;
  string16 name2_;
  string16 value1_;
  string16 value2_;
  int unique_id1_, unique_id2_;
  const TimeDelta test_timeout_;
  scoped_refptr<AutofillDBThreadObserverHelper> observer_helper_;
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
              Source<WebDataService>(wds_.get()),
              Property(&Details<const AutofillChangeList>::ptr,
                       Pointee(ElementsAreArray(expected_changes))))).
      WillOnce(SignalEvent(&done_event_));

  std::vector<webkit_glue::FormField> form_fields;
  AppendFormField(name1_, value1_, &form_fields);
  AppendFormField(name2_, value2_, &form_fields);
  wds_->AddFormFields(form_fields);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);

  AutofillWebDataServiceConsumer<std::vector<string16> > consumer;
  WebDataService::Handle handle;
  static const int limit = 10;
  handle = wds_->GetFormValuesForElementName(
      name1_, string16(), limit, &consumer);

  // The message loop will exit when the consumer is called.
  MessageLoop::current()->Run();

  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(1U, consumer.result().size());
  EXPECT_EQ(value1_, consumer.result()[0]);
}

TEST_F(WebDataServiceAutofillTest, FormFillRemoveOne) {
  // First add some values to autofill.
  EXPECT_CALL(*observer_helper_->observer(), Observe(_, _, _)).
      WillOnce(SignalEvent(&done_event_));
  std::vector<webkit_glue::FormField> form_fields;
  AppendFormField(name1_, value1_, &form_fields);
  wds_->AddFormFields(form_fields);

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
              Source<WebDataService>(wds_.get()),
              Property(&Details<const AutofillChangeList>::ptr,
                       Pointee(ElementsAreArray(expected_changes))))).
      WillOnce(SignalEvent(&done_event_));
  wds_->RemoveFormValueForElementName(name1_, value1_);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);
}

TEST_F(WebDataServiceAutofillTest, FormFillRemoveMany) {
  TimeDelta one_day(TimeDelta::FromDays(1));
  Time t = Time::Now();

  EXPECT_CALL(*observer_helper_->observer(), Observe(_, _, _)).
      WillOnce(SignalEvent(&done_event_));
  std::vector<webkit_glue::FormField> form_fields;
  AppendFormField(name1_, value1_, &form_fields);
  AppendFormField(name2_, value2_, &form_fields);
  wds_->AddFormFields(form_fields);

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
              Source<WebDataService>(wds_.get()),
              Property(&Details<const AutofillChangeList>::ptr,
                       Pointee(ElementsAreArray(expected_changes))))).
      WillOnce(SignalEvent(&done_event_));
  wds_->RemoveFormElementsAddedBetween(t, t + one_day);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);
}

TEST_F(WebDataServiceAutofillTest, ProfileAdd) {
  AutoFillProfile profile;

  // Check that GUID-based notification was sent.
  const AutofillProfileChange expected_change(
      AutofillProfileChange::ADD, profile.guid(), &profile);
  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(NotificationType(NotificationType::AUTOFILL_PROFILE_CHANGED),
              Source<WebDataService>(wds_.get()),
              Property(&Details<const AutofillProfileChange>::ptr,
                       Pointee(expected_change)))).
      WillOnce(SignalEvent(&done_event_));

  wds_->AddAutoFillProfile(profile);
  done_event_.TimedWait(test_timeout_);

  // Check that it was added.
  AutofillWebDataServiceConsumer<std::vector<AutoFillProfile*> > consumer;
  WebDataService::Handle handle = wds_->GetAutoFillProfiles(&consumer);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(1U, consumer.result().size());
  EXPECT_EQ(profile, *consumer.result()[0]);
  STLDeleteElements(&consumer.result());
}

TEST_F(WebDataServiceAutofillTest, ProfileRemove) {
  AutoFillProfile profile;

  // Add a profile.
  EXPECT_CALL(*observer_helper_->observer(), Observe(_, _, _)).
      Times(1).
      WillOnce(SignalEvent(&done_event_));
  wds_->AddAutoFillProfile(profile);
  done_event_.TimedWait(test_timeout_);

  // Check that it was added.
  AutofillWebDataServiceConsumer<std::vector<AutoFillProfile*> > consumer;
  WebDataService::Handle handle = wds_->GetAutoFillProfiles(&consumer);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(1U, consumer.result().size());
  EXPECT_EQ(profile, *consumer.result()[0]);
  STLDeleteElements(&consumer.result());

  // Check that GUID-based notification was sent.
  const AutofillProfileChange expected_change(
      AutofillProfileChange::REMOVE, profile.guid(), NULL);
  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(NotificationType(NotificationType::AUTOFILL_PROFILE_CHANGED),
              Source<WebDataService>(wds_.get()),
              Property(&Details<const AutofillProfileChange>::ptr,
                       Pointee(expected_change)))).
      WillOnce(SignalEvent(&done_event_));

  // Remove the profile.
  wds_->RemoveAutoFillProfile(profile.guid());
  done_event_.TimedWait(test_timeout_);

  // Check that it was removed.
  AutofillWebDataServiceConsumer<std::vector<AutoFillProfile*> > consumer2;
  WebDataService::Handle handle2 = wds_->GetAutoFillProfiles(&consumer2);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle2, consumer2.handle());
  ASSERT_EQ(0U, consumer2.result().size());
}

TEST_F(WebDataServiceAutofillTest, ProfileUpdate) {
  AutoFillProfile profile1;
  profile1.SetInfo(AutofillType(NAME_FIRST), ASCIIToUTF16("Abe"));
  AutoFillProfile profile2;
  profile2.SetInfo(AutofillType(NAME_FIRST), ASCIIToUTF16("Alice"));

  EXPECT_CALL(*observer_helper_->observer(), Observe(_, _, _)).
      WillOnce(DoDefault()).
      WillOnce(SignalEvent(&done_event_));
  wds_->AddAutoFillProfile(profile1);
  wds_->AddAutoFillProfile(profile2);
  done_event_.TimedWait(test_timeout_);

  // Check that they were added.
  AutofillWebDataServiceConsumer<std::vector<AutoFillProfile*> > consumer;
  WebDataService::Handle handle = wds_->GetAutoFillProfiles(&consumer);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(2U, consumer.result().size());
  EXPECT_EQ(profile1, *consumer.result()[0]);
  EXPECT_EQ(profile2, *consumer.result()[1]);
  STLDeleteElements(&consumer.result());

  AutoFillProfile profile1_changed(profile1);
  profile1_changed.SetInfo(AutofillType(NAME_FIRST), ASCIIToUTF16("Bill"));
  const AutofillProfileChange expected_change(
      AutofillProfileChange::UPDATE, profile1.guid(), &profile1_changed);

  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(NotificationType(NotificationType::AUTOFILL_PROFILE_CHANGED),
              Source<WebDataService>(wds_.get()),
              Property(&Details<const AutofillProfileChange>::ptr,
                       Pointee(expected_change)))).
      WillOnce(SignalEvent(&done_event_));

  // Update the profile.
  wds_->UpdateAutoFillProfile(profile1_changed);
  done_event_.TimedWait(test_timeout_);

  // Check that the updates were made.
  AutofillWebDataServiceConsumer<std::vector<AutoFillProfile*> > consumer2;
  WebDataService::Handle handle2 = wds_->GetAutoFillProfiles(&consumer2);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle2, consumer2.handle());
  ASSERT_EQ(2U, consumer2.result().size());
  EXPECT_NE(profile1, *consumer2.result()[0]);
  EXPECT_EQ(profile1_changed, *consumer2.result()[0]);
  EXPECT_EQ(profile2, *consumer2.result()[1]);
  STLDeleteElements(&consumer2.result());
}

TEST_F(WebDataServiceAutofillTest, CreditAdd) {
  CreditCard card;
  const AutofillCreditCardChange expected_change(
      AutofillCreditCardChange::ADD, card.guid(), &card);

  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(
          NotificationType(NotificationType::AUTOFILL_CREDIT_CARD_CHANGED),
              Source<WebDataService>(wds_.get()),
              Property(&Details<const AutofillCreditCardChange>::ptr,
                       Pointee(expected_change)))).
      WillOnce(SignalEvent(&done_event_));

  wds_->AddCreditCard(card);
  done_event_.TimedWait(test_timeout_);

  // Check that it was added.
  AutofillWebDataServiceConsumer<std::vector<CreditCard*> > consumer;
  WebDataService::Handle handle = wds_->GetCreditCards(&consumer);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(1U, consumer.result().size());
  EXPECT_EQ(card, *consumer.result()[0]);
  STLDeleteElements(&consumer.result());
}

TEST_F(WebDataServiceAutofillTest, CreditCardRemove) {
  CreditCard credit_card;

  // Add a credit card.
  EXPECT_CALL(*observer_helper_->observer(), Observe(_, _, _)).
      WillOnce(SignalEvent(&done_event_));
  wds_->AddCreditCard(credit_card);
  done_event_.TimedWait(test_timeout_);

  // Check that it was added.
  AutofillWebDataServiceConsumer<std::vector<CreditCard*> > consumer;
  WebDataService::Handle handle = wds_->GetCreditCards(&consumer);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(1U, consumer.result().size());
  EXPECT_EQ(credit_card, *consumer.result()[0]);
  STLDeleteElements(&consumer.result());

  // Remove the credit card.
  const AutofillCreditCardChange expected_change(
      AutofillCreditCardChange::REMOVE, credit_card.guid(), NULL);
  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(
          NotificationType(NotificationType::AUTOFILL_CREDIT_CARD_CHANGED),
              Source<WebDataService>(wds_.get()),
              Property(&Details<const AutofillCreditCardChange>::ptr,
                       Pointee(expected_change)))).
      WillOnce(SignalEvent(&done_event_));
  wds_->RemoveCreditCard(credit_card.guid());
  done_event_.TimedWait(test_timeout_);

  // Check that it was removed.
  AutofillWebDataServiceConsumer<std::vector<CreditCard*> > consumer2;
  WebDataService::Handle handle2 = wds_->GetCreditCards(&consumer2);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle2, consumer2.handle());
  ASSERT_EQ(0U, consumer2.result().size());
}

TEST_F(WebDataServiceAutofillTest, CreditUpdate) {
  CreditCard card1;
  card1.SetInfo(AutofillType(CREDIT_CARD_NAME), ASCIIToUTF16("Abe"));
  CreditCard card2;
  card2.SetInfo(AutofillType(CREDIT_CARD_NAME), ASCIIToUTF16("Alice"));

  EXPECT_CALL(*observer_helper_->observer(), Observe(_, _, _)).
      Times(2).
      WillOnce(DoDefault()).
      WillOnce(SignalEvent(&done_event_));
  wds_->AddCreditCard(card1);
  wds_->AddCreditCard(card2);
  done_event_.TimedWait(test_timeout_);

  // Check that they got added.
  AutofillWebDataServiceConsumer<std::vector<CreditCard*> > consumer;
  WebDataService::Handle handle = wds_->GetCreditCards(&consumer);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(2U, consumer.result().size());
  EXPECT_EQ(card1, *consumer.result()[0]);
  EXPECT_EQ(card2, *consumer.result()[1]);
  STLDeleteElements(&consumer.result());

  CreditCard card1_changed(card1);
  card1_changed.SetInfo(AutofillType(CREDIT_CARD_NAME), ASCIIToUTF16("Bill"));
  const AutofillCreditCardChange expected_change(
      AutofillCreditCardChange::UPDATE, card1.guid(), &card1_changed);

  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(
          NotificationType(NotificationType::AUTOFILL_CREDIT_CARD_CHANGED),
              Source<WebDataService>(wds_.get()),
              Property(&Details<const AutofillCreditCardChange>::ptr,
                       Pointee(expected_change)))).
      WillOnce(SignalEvent(&done_event_));

  wds_->UpdateCreditCard(card1_changed);
  done_event_.TimedWait(test_timeout_);

  // Check that the updates were made.
  AutofillWebDataServiceConsumer<std::vector<CreditCard*> > consumer2;
  WebDataService::Handle handle2 = wds_->GetCreditCards(&consumer2);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle2, consumer2.handle());
  ASSERT_EQ(2U, consumer2.result().size());
  EXPECT_NE(card1, *consumer2.result()[0]);
  EXPECT_EQ(card1_changed, *consumer2.result()[0]);
  EXPECT_EQ(card2, *consumer2.result()[1]);
  STLDeleteElements(&consumer2.result());
}
