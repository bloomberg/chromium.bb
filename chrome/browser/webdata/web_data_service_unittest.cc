// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/autofill_table.h"
#include "chrome/browser/webdata/autofill_web_data_service.h"
#include "chrome/browser/webdata/autofill_web_data_service_observer.h"
#include "chrome/browser/webdata/web_data_service_test_util.h"
#include "chrome/browser/webdata/web_database_service.h"
#include "components/autofill/browser/autofill_country.h"
#include "components/autofill/browser/autofill_profile.h"
#include "components/autofill/browser/credit_card.h"
#include "components/autofill/common/form_field_data.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using base::WaitableEvent;
using content::BrowserThread;
using testing::_;
using testing::DoDefault;
using testing::ElementsAreArray;
using testing::Pointee;
using testing::Property;

static const int kWebDataServiceTimeoutSeconds = 8;

ACTION_P(SignalEvent, event) {
  event->Signal();
}

class MockAutofillWebDataServiceObserver
    : public AutofillWebDataServiceObserverOnDBThread {
 public:
  MOCK_METHOD1(AutofillEntriesChanged,
               void(const AutofillChangeList& changes));
  MOCK_METHOD1(AutofillProfileChanged,
               void(const AutofillProfileChange& change));
};

class WebDataServiceTest : public testing::Test {
 public:
  WebDataServiceTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB) {}

 protected:
  virtual void SetUp() {
    db_thread_.Start();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath path = temp_dir_.path().AppendASCII("TestWebDB");

    wdbs_ = new WebDatabaseService(path, AutofillCountry::ApplicationLocale());
    wdbs_->AddTable(scoped_ptr<WebDatabaseTable>(new AutofillTable()));
    wdbs_->LoadDatabase(WebDatabaseService::InitCallback());

    wds_ = new AutofillWebDataService(
        wdbs_, WebDataServiceBase::ProfileErrorCallback());
    wds_->Init();
  }

  virtual void TearDown() {
    wds_->ShutdownOnUIThread();
    wdbs_->ShutdownDatabase();
    wds_ = NULL;
    wdbs_ = NULL;
    WaitForDatabaseThread();

    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    MessageLoop::current()->Run();
    db_thread_.Stop();
  }

  void WaitForDatabaseThread() {
    base::WaitableEvent done(false, false);
    BrowserThread::PostTask(
        BrowserThread::DB,
        FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done)));
    done.Wait();
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  base::FilePath profile_dir_;
  scoped_refptr<AutofillWebDataService> wds_;
  scoped_refptr<WebDatabaseService> wdbs_;
  base::ScopedTempDir temp_dir_;
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

    void(AutofillWebDataService::*add_observer_func)(
        AutofillWebDataServiceObserverOnDBThread*) =
        &AutofillWebDataService::AddObserver;
    BrowserThread::PostTask(
        BrowserThread::DB,
        FROM_HERE,
        base::Bind(add_observer_func, wds_, &observer_));
    WaitForDatabaseThread();
  }

  virtual void TearDown() {
    void(AutofillWebDataService::*remove_observer_func)(
        AutofillWebDataServiceObserverOnDBThread*) =
        &AutofillWebDataService::RemoveObserver;
    BrowserThread::PostTask(
        BrowserThread::DB,
        FROM_HERE,
        base::Bind(remove_observer_func, wds_, &observer_));
    WaitForDatabaseThread();

    WebDataServiceTest::TearDown();
  }

  void AppendFormField(const string16& name,
                       const string16& value,
                       std::vector<FormFieldData>* form_fields) {
    FormFieldData field;
    field.name = name;
    field.value = value;
    form_fields->push_back(field);
  }

  string16 name1_;
  string16 name2_;
  string16 value1_;
  string16 value2_;
  int unique_id1_, unique_id2_;
  const TimeDelta test_timeout_;
  testing::NiceMock<MockAutofillWebDataServiceObserver> observer_;
  WaitableEvent done_event_;
};

// Simple consumer for Keywords data. Stores the result data and quits UI
// message loop when callback is invoked.
class KeywordsConsumer : public WebDataServiceConsumer {
 public:
  KeywordsConsumer() : load_succeeded(false) {}

  virtual void OnWebDataServiceRequestDone(
      WebDataService::Handle h,
      const WDTypedResult* result) OVERRIDE {
    if (result) {
      load_succeeded = true;
      DCHECK(result->GetType() == KEYWORDS_RESULT);
      keywords_result =
          reinterpret_cast<const WDResult<WDKeywordsResult>*>(
              result)->GetValue();
    }

    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    MessageLoop::current()->Quit();
  }

  // True if keywords data was loaded successfully.
  bool load_succeeded;
  // Result data from completion callback.
  WDKeywordsResult keywords_result;
};

TEST_F(WebDataServiceAutofillTest, FormFillAdd) {
  const AutofillChange expected_changes[] = {
    AutofillChange(AutofillChange::ADD, AutofillKey(name1_, value1_)),
    AutofillChange(AutofillChange::ADD, AutofillKey(name2_, value2_))
  };

  // This will verify that the correct notification is triggered,
  // passing the correct list of autofill keys in the details.
  EXPECT_CALL(observer_,
              AutofillEntriesChanged(ElementsAreArray(expected_changes)))
      .WillOnce(SignalEvent(&done_event_));

  std::vector<FormFieldData> form_fields;
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
  EXPECT_CALL(observer_, AutofillEntriesChanged(_))
      .WillOnce(SignalEvent(&done_event_));
  std::vector<FormFieldData> form_fields;
  AppendFormField(name1_, value1_, &form_fields);
  wds_->AddFormFields(form_fields);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);

  // This will verify that the correct notification is triggered,
  // passing the correct list of autofill keys in the details.
  const AutofillChange expected_changes[] = {
    AutofillChange(AutofillChange::REMOVE, AutofillKey(name1_, value1_))
  };
  EXPECT_CALL(observer_,
              AutofillEntriesChanged(ElementsAreArray(expected_changes)))
      .WillOnce(SignalEvent(&done_event_));
  wds_->RemoveFormValueForElementName(name1_, value1_);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);
}

TEST_F(WebDataServiceAutofillTest, FormFillRemoveMany) {
  TimeDelta one_day(TimeDelta::FromDays(1));
  Time t = Time::Now();

  EXPECT_CALL(observer_, AutofillEntriesChanged(_))
      .WillOnce(SignalEvent(&done_event_));

  std::vector<FormFieldData> form_fields;
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
  EXPECT_CALL(observer_,
              AutofillEntriesChanged(ElementsAreArray(expected_changes)))
      .WillOnce(SignalEvent(&done_event_));
  wds_->RemoveFormElementsAddedBetween(t, t + one_day);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);
}

TEST_F(WebDataServiceAutofillTest, ProfileAdd) {
  AutofillProfile profile;

  // Check that GUID-based notification was sent.
  const AutofillProfileChange expected_change(
      AutofillProfileChange::ADD, profile.guid(), &profile);
  EXPECT_CALL(observer_, AutofillProfileChanged(expected_change))
      .WillOnce(SignalEvent(&done_event_));

  wds_->AddAutofillProfile(profile);
  done_event_.TimedWait(test_timeout_);

  // Check that it was added.
  AutofillWebDataServiceConsumer<std::vector<AutofillProfile*> > consumer;
  WebDataService::Handle handle = wds_->GetAutofillProfiles(&consumer);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(1U, consumer.result().size());
  EXPECT_EQ(profile, *consumer.result()[0]);
  STLDeleteElements(&consumer.result());
}

TEST_F(WebDataServiceAutofillTest, ProfileRemove) {
  AutofillProfile profile;

  // Add a profile.
  EXPECT_CALL(observer_, AutofillProfileChanged(_))
      .WillOnce(SignalEvent(&done_event_));
  wds_->AddAutofillProfile(profile);
  done_event_.TimedWait(test_timeout_);

  // Check that it was added.
  AutofillWebDataServiceConsumer<std::vector<AutofillProfile*> > consumer;
  WebDataService::Handle handle = wds_->GetAutofillProfiles(&consumer);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(1U, consumer.result().size());
  EXPECT_EQ(profile, *consumer.result()[0]);
  STLDeleteElements(&consumer.result());

  // Check that GUID-based notification was sent.
  const AutofillProfileChange expected_change(
      AutofillProfileChange::REMOVE, profile.guid(), NULL);
  EXPECT_CALL(observer_, AutofillProfileChanged(expected_change))
      .WillOnce(SignalEvent(&done_event_));

  // Remove the profile.
  wds_->RemoveAutofillProfile(profile.guid());
  done_event_.TimedWait(test_timeout_);

  // Check that it was removed.
  AutofillWebDataServiceConsumer<std::vector<AutofillProfile*> > consumer2;
  WebDataService::Handle handle2 = wds_->GetAutofillProfiles(&consumer2);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle2, consumer2.handle());
  ASSERT_EQ(0U, consumer2.result().size());
}

TEST_F(WebDataServiceAutofillTest, ProfileUpdate) {
  AutofillProfile profile1;
  profile1.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Abe"));
  AutofillProfile profile2;
  profile2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Alice"));

  EXPECT_CALL(observer_, AutofillProfileChanged(_))
      .WillOnce(DoDefault())
      .WillOnce(SignalEvent(&done_event_));

  wds_->AddAutofillProfile(profile1);
  wds_->AddAutofillProfile(profile2);
  done_event_.TimedWait(test_timeout_);

  // Check that they were added.
  AutofillWebDataServiceConsumer<std::vector<AutofillProfile*> > consumer;
  WebDataService::Handle handle = wds_->GetAutofillProfiles(&consumer);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(2U, consumer.result().size());
  EXPECT_EQ(profile1, *consumer.result()[0]);
  EXPECT_EQ(profile2, *consumer.result()[1]);
  STLDeleteElements(&consumer.result());

  AutofillProfile profile1_changed(profile1);
  profile1_changed.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Bill"));
  const AutofillProfileChange expected_change(
      AutofillProfileChange::UPDATE, profile1.guid(), &profile1_changed);

  EXPECT_CALL(observer_, AutofillProfileChanged(expected_change))
      .WillOnce(SignalEvent(&done_event_));

  // Update the profile.
  wds_->UpdateAutofillProfile(profile1_changed);
  done_event_.TimedWait(test_timeout_);

  // Check that the updates were made.
  AutofillWebDataServiceConsumer<std::vector<AutofillProfile*> > consumer2;
  WebDataService::Handle handle2 = wds_->GetAutofillProfiles(&consumer2);
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
  wds_->AddCreditCard(card);
  WaitForDatabaseThread();

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
  wds_->AddCreditCard(credit_card);
  WaitForDatabaseThread();

  // Check that it was added.
  AutofillWebDataServiceConsumer<std::vector<CreditCard*> > consumer;
  WebDataService::Handle handle = wds_->GetCreditCards(&consumer);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle, consumer.handle());
  ASSERT_EQ(1U, consumer.result().size());
  EXPECT_EQ(credit_card, *consumer.result()[0]);
  STLDeleteElements(&consumer.result());

  // Remove the credit card.
  wds_->RemoveCreditCard(credit_card.guid());
  WaitForDatabaseThread();

  // Check that it was removed.
  AutofillWebDataServiceConsumer<std::vector<CreditCard*> > consumer2;
  WebDataService::Handle handle2 = wds_->GetCreditCards(&consumer2);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle2, consumer2.handle());
  ASSERT_EQ(0U, consumer2.result().size());
}

TEST_F(WebDataServiceAutofillTest, CreditUpdate) {
  CreditCard card1;
  card1.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Abe"));
  CreditCard card2;
  card2.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Alice"));

  wds_->AddCreditCard(card1);
  wds_->AddCreditCard(card2);
  WaitForDatabaseThread();

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
  card1_changed.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Bill"));

  wds_->UpdateCreditCard(card1_changed);
  WaitForDatabaseThread();

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

TEST_F(WebDataServiceAutofillTest, AutofillRemoveModifiedBetween) {
  // Add a profile.
  EXPECT_CALL(observer_, AutofillProfileChanged(_))
      .WillOnce(SignalEvent(&done_event_));
  AutofillProfile profile;
  wds_->AddAutofillProfile(profile);
  done_event_.TimedWait(test_timeout_);

  // Check that it was added.
  AutofillWebDataServiceConsumer<std::vector<AutofillProfile*> >
      profile_consumer;
  WebDataService::Handle handle = wds_->GetAutofillProfiles(&profile_consumer);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle, profile_consumer.handle());
  ASSERT_EQ(1U, profile_consumer.result().size());
  EXPECT_EQ(profile, *profile_consumer.result()[0]);
  STLDeleteElements(&profile_consumer.result());

  // Add a credit card.
  CreditCard credit_card;
  wds_->AddCreditCard(credit_card);
  WaitForDatabaseThread();

  // Check that it was added.
  AutofillWebDataServiceConsumer<std::vector<CreditCard*> > card_consumer;
  handle = wds_->GetCreditCards(&card_consumer);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle, card_consumer.handle());
  ASSERT_EQ(1U, card_consumer.result().size());
  EXPECT_EQ(credit_card, *card_consumer.result()[0]);
  STLDeleteElements(&card_consumer.result());

  // Check that GUID-based notification was sent for the profile.
  const AutofillProfileChange expected_profile_change(
      AutofillProfileChange::REMOVE, profile.guid(), NULL);
  EXPECT_CALL(observer_, AutofillProfileChanged(expected_profile_change))
      .WillOnce(SignalEvent(&done_event_));

  // Remove the profile using time range of "all time".
  wds_->RemoveAutofillDataModifiedBetween(Time(), Time());
  done_event_.TimedWait(test_timeout_);
  WaitForDatabaseThread();

  // Check that the profile was removed.
  AutofillWebDataServiceConsumer<std::vector<AutofillProfile*> >
      profile_consumer2;
  WebDataService::Handle handle2 =
      wds_->GetAutofillProfiles(&profile_consumer2);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle2, profile_consumer2.handle());
  ASSERT_EQ(0U, profile_consumer2.result().size());

  // Check that the credit card was removed.
  AutofillWebDataServiceConsumer<std::vector<CreditCard*> > card_consumer2;
  handle2 = wds_->GetCreditCards(&card_consumer2);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle2, card_consumer2.handle());
  ASSERT_EQ(0U, card_consumer2.result().size());
}
