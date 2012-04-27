// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/intents/default_web_intent_service.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/browser/webdata/web_data_service_test_util.h"
#include "chrome/browser/webdata/web_intents_table.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/guid.h"
#include "chrome/test/base/thread_observer_helper.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/forms/form_field.h"

using base::Time;
using base::TimeDelta;
using base::WaitableEvent;
using content::BrowserThread;
using testing::_;
using testing::DoDefault;
using testing::ElementsAreArray;
using testing::Pointee;
using testing::Property;
using webkit_glue::WebIntentServiceData;

typedef std::vector<AutofillChange> AutofillChangeList;

static const int kWebDataServiceTimeoutSeconds = 8;

ACTION_P(SignalEvent, event) {
  event->Signal();
}

class AutofillDBThreadObserverHelper : public DBThreadObserverHelper {
 protected:
  virtual ~AutofillDBThreadObserverHelper() {}

  virtual void RegisterObservers() {
    registrar_.Add(&observer_,
                   chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED,
                   content::NotificationService::AllSources());
    registrar_.Add(&observer_,
                   chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED,
                   content::NotificationService::AllSources());
    registrar_.Add(&observer_,
                   chrome::NOTIFICATION_AUTOFILL_CREDIT_CARD_CHANGED,
                   content::NotificationService::AllSources());
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

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    wds_ = new WebDataService();
    wds_->Init(temp_dir_.path());
  }

  virtual void TearDown() {
    if (wds_.get())
      wds_->Shutdown();

    db_thread_.Stop();
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    MessageLoop::current()->Run();
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  FilePath profile_dir_;
  scoped_refptr<WebDataService> wds_;
  ScopedTempDir temp_dir_;
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
                       std::vector<webkit::forms::FormField>* form_fields) {
    webkit::forms::FormField field;
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
  scoped_refptr<AutofillDBThreadObserverHelper> observer_helper_;
  WaitableEvent done_event_;
};

// Run the current message loop. OnWebDataServiceRequestDone will invoke
// MessageLoop::Quit on completion, so this call will finish at that point.
static void WaitUntilCalled() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  MessageLoop::current()->Run();
}

// Simple consumer for WebIntents service data. Stores the result data and
// quits UI message loop when callback is invoked.
class WebIntentsConsumer : public WebDataServiceConsumer {
 public:
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result) {
    services_.clear();
    if (result) {
      DCHECK(result->GetType() == WEB_INTENTS_RESULT);
      services_ = static_cast<
          const WDResult<std::vector<WebIntentServiceData> >*>(result)->
              GetValue();
    }

    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    MessageLoop::current()->Quit();
  }

  // Result data from completion callback.
  std::vector<WebIntentServiceData> services_;
};

// Simple consumer for WebIntents defaults data. Stores the result data and
// quits UI message loop when callback is invoked.
class WebIntentsDefaultsConsumer : public WebDataServiceConsumer {
 public:
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result) {
    services_.clear();
    if (result) {
      DCHECK(result->GetType() == WEB_INTENTS_DEFAULTS_RESULT);
      services_ = static_cast<
          const WDResult<std::vector<DefaultWebIntentService> >*>(result)->
              GetValue();
    }

    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    MessageLoop::current()->Quit();
  }

  // Result data from completion callback.
  std::vector<DefaultWebIntentService> services_;
};

// Simple consumer for Keywords data. Stores the result data and quits UI
// message loop when callback is invoked.
class KeywordsConsumer : public WebDataServiceConsumer {
 public:
  KeywordsConsumer() : load_succeeded(false) {}

  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result) {
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
  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(int(chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED),
              content::Source<WebDataService>(wds_.get()),
              Property(&content::Details<const AutofillChangeList>::ptr,
                       Pointee(ElementsAreArray(expected_changes))))).
      WillOnce(SignalEvent(&done_event_));

  std::vector<webkit::forms::FormField> form_fields;
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
  std::vector<webkit::forms::FormField> form_fields;
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
      Observe(int(chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED),
              content::Source<WebDataService>(wds_.get()),
              Property(&content::Details<const AutofillChangeList>::ptr,
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
  std::vector<webkit::forms::FormField> form_fields;
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
      Observe(int(chrome::NOTIFICATION_AUTOFILL_ENTRIES_CHANGED),
              content::Source<WebDataService>(wds_.get()),
              Property(&content::Details<const AutofillChangeList>::ptr,
                       Pointee(ElementsAreArray(expected_changes))))).
      WillOnce(SignalEvent(&done_event_));
  wds_->RemoveFormElementsAddedBetween(t, t + one_day);

  // The event will be signaled when the mock observer is notified.
  done_event_.TimedWait(test_timeout_);
}

TEST_F(WebDataServiceAutofillTest, ProfileAdd) {
  AutofillProfile profile;

  // Check that GUID-based notification was sent.
  const AutofillProfileChange expected_change(
      AutofillProfileChange::ADD, profile.guid(), &profile);
  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(int(chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED),
              content::Source<WebDataService>(wds_.get()),
              Property(&content::Details<const AutofillProfileChange>::ptr,
                       Pointee(expected_change)))).
      WillOnce(SignalEvent(&done_event_));

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
  EXPECT_CALL(*observer_helper_->observer(), Observe(_, _, _)).
      Times(1).
      WillOnce(SignalEvent(&done_event_));
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
  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(int(chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED),
              content::Source<WebDataService>(wds_.get()),
              Property(&content::Details<const AutofillProfileChange>::ptr,
                       Pointee(expected_change)))).
      WillOnce(SignalEvent(&done_event_));

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
  profile1.SetInfo(NAME_FIRST, ASCIIToUTF16("Abe"));
  AutofillProfile profile2;
  profile2.SetInfo(NAME_FIRST, ASCIIToUTF16("Alice"));

  EXPECT_CALL(*observer_helper_->observer(), Observe(_, _, _)).
      WillOnce(DoDefault()).
      WillOnce(SignalEvent(&done_event_));
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
  profile1_changed.SetInfo(NAME_FIRST, ASCIIToUTF16("Bill"));
  const AutofillProfileChange expected_change(
      AutofillProfileChange::UPDATE, profile1.guid(), &profile1_changed);

  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(int(chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED),
              content::Source<WebDataService>(wds_.get()),
              Property(&content::Details<const AutofillProfileChange>::ptr,
                       Pointee(expected_change)))).
      WillOnce(SignalEvent(&done_event_));

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
  const AutofillCreditCardChange expected_change(
      AutofillCreditCardChange::ADD, card.guid(), &card);

  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(
          int(chrome::NOTIFICATION_AUTOFILL_CREDIT_CARD_CHANGED),
              content::Source<WebDataService>(wds_.get()),
              Property(&content::Details<const AutofillCreditCardChange>::ptr,
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
          int(chrome::NOTIFICATION_AUTOFILL_CREDIT_CARD_CHANGED),
              content::Source<WebDataService>(wds_.get()),
              Property(&content::Details<const AutofillCreditCardChange>::ptr,
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
  card1.SetInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Abe"));
  CreditCard card2;
  card2.SetInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Alice"));

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
  card1_changed.SetInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Bill"));
  const AutofillCreditCardChange expected_change(
      AutofillCreditCardChange::UPDATE, card1.guid(), &card1_changed);

  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(
          int(chrome::NOTIFICATION_AUTOFILL_CREDIT_CARD_CHANGED),
              content::Source<WebDataService>(wds_.get()),
              Property(&content::Details<const AutofillCreditCardChange>::ptr,
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

TEST_F(WebDataServiceAutofillTest, AutofillRemoveModifiedBetween) {
  // Add a profile.
  EXPECT_CALL(*observer_helper_->observer(), Observe(_, _, _)).
      Times(1).
      WillOnce(SignalEvent(&done_event_));
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
  EXPECT_CALL(*observer_helper_->observer(), Observe(_, _, _)).
      WillOnce(SignalEvent(&done_event_));
  CreditCard credit_card;
  wds_->AddCreditCard(credit_card);
  done_event_.TimedWait(test_timeout_);

  // Check that it was added.
  AutofillWebDataServiceConsumer<std::vector<CreditCard*> >
      card_consumer;
  handle = wds_->GetCreditCards(&card_consumer);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle, card_consumer.handle());
  ASSERT_EQ(1U, card_consumer.result().size());
  EXPECT_EQ(credit_card, *card_consumer.result()[0]);
  STLDeleteElements(&card_consumer.result());

  // Check that GUID-based notification was sent for the profile.
  const AutofillProfileChange expected_profile_change(
      AutofillProfileChange::REMOVE, profile.guid(), NULL);
  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(int(chrome::NOTIFICATION_AUTOFILL_PROFILE_CHANGED),
              content::Source<WebDataService>(wds_.get()),
              Property(&content::Details<const AutofillProfileChange>::ptr,
                       Pointee(expected_profile_change)))).
      WillOnce(SignalEvent(&done_event_));

  // Check that GUID-based notification was sent for the credit card.
  const AutofillCreditCardChange expected_card_change(
      AutofillCreditCardChange::REMOVE, credit_card.guid(), NULL);
  EXPECT_CALL(
      *observer_helper_->observer(),
      Observe(
          int(chrome::NOTIFICATION_AUTOFILL_CREDIT_CARD_CHANGED),
              content::Source<WebDataService>(wds_.get()),
              Property(&content::Details<const AutofillCreditCardChange>::ptr,
                       Pointee(expected_card_change)))).
      WillOnce(SignalEvent(&done_event_));

  // Remove the profile using time range of "all time".
  wds_->RemoveAutofillProfilesAndCreditCardsModifiedBetween(Time(), Time());
  done_event_.TimedWait(test_timeout_);
  done_event_.TimedWait(test_timeout_);

  // Check that the profile was removed.
  AutofillWebDataServiceConsumer<std::vector<AutofillProfile*> >
      profile_consumer2;
  WebDataService::Handle handle2 =
      wds_->GetAutofillProfiles(&profile_consumer2);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle2, profile_consumer2.handle());
  ASSERT_EQ(0U, profile_consumer2.result().size());

  // Check that the credit card was removed.
  AutofillWebDataServiceConsumer<std::vector<CreditCard*> >
      card_consumer2;
  handle2 = wds_->GetCreditCards(&card_consumer2);
  MessageLoop::current()->Run();
  EXPECT_EQ(handle2, card_consumer2.handle());
  ASSERT_EQ(0U, card_consumer2.result().size());
}

TEST_F(WebDataServiceTest, WebIntents) {
  WebIntentsConsumer consumer;

  wds_->GetWebIntentServices(ASCIIToUTF16("share"), &consumer);
  WaitUntilCalled();
  EXPECT_EQ(0U, consumer.services_.size());

  WebIntentServiceData service;
  service.service_url = GURL("http://google.com");
  service.action = ASCIIToUTF16("share1");
  service.type = ASCIIToUTF16("image/*");
  wds_->AddWebIntentService(service);

  service.action = ASCIIToUTF16("share");
  service.type = ASCIIToUTF16("image/*");
  wds_->AddWebIntentService(service);

  service.type = ASCIIToUTF16("video/*");
  wds_->AddWebIntentService(service);

  wds_->GetWebIntentServices(ASCIIToUTF16("share"), &consumer);
  WaitUntilCalled();
  ASSERT_EQ(2U, consumer.services_.size());

  if (consumer.services_[0].type != ASCIIToUTF16("image/*"))
    std::swap(consumer.services_[0], consumer.services_[1]);

  EXPECT_EQ(service.service_url, consumer.services_[0].service_url);
  EXPECT_EQ(service.action, consumer.services_[0].action);
  EXPECT_EQ(ASCIIToUTF16("image/*"), consumer.services_[0].type);
  EXPECT_EQ(service.service_url, consumer.services_[1].service_url);
  EXPECT_EQ(service.action, consumer.services_[1].action);
  EXPECT_EQ(service.type, consumer.services_[1].type);

  service.type = ASCIIToUTF16("image/*");
  wds_->RemoveWebIntentService(service);

  wds_->GetWebIntentServices(ASCIIToUTF16("share"), &consumer);
  WaitUntilCalled();
  ASSERT_EQ(1U, consumer.services_.size());

  service.type = ASCIIToUTF16("video/*");
  EXPECT_EQ(service.service_url, consumer.services_[0].service_url);
  EXPECT_EQ(service.action, consumer.services_[0].action);
  EXPECT_EQ(service.type, consumer.services_[0].type);
}

TEST_F(WebDataServiceTest, WebIntentsForURL) {
  WebIntentsConsumer consumer;

  WebIntentServiceData service;
  service.service_url = GURL("http://google.com");
  service.action = ASCIIToUTF16("share1");
  service.type = ASCIIToUTF16("image/*");
  wds_->AddWebIntentService(service);

  service.action = ASCIIToUTF16("share");
  service.type = ASCIIToUTF16("image/*");
  wds_->AddWebIntentService(service);

  wds_->GetWebIntentServicesForURL(
      UTF8ToUTF16(service.service_url.spec()),
      &consumer);
  WaitUntilCalled();
  ASSERT_EQ(2U, consumer.services_.size());
  EXPECT_EQ(service, consumer.services_[0]);
  service.action = ASCIIToUTF16("share1");
  EXPECT_EQ(service, consumer.services_[1]);
}

TEST_F(WebDataServiceTest, WebIntentsGetAll) {
  WebIntentsConsumer consumer;

  WebIntentServiceData service;
  service.service_url = GURL("http://google.com");
  service.action = ASCIIToUTF16("share");
  service.type = ASCIIToUTF16("image/*");
  wds_->AddWebIntentService(service);

  service.action = ASCIIToUTF16("edit");
  wds_->AddWebIntentService(service);

  wds_->GetAllWebIntentServices(&consumer);
  WaitUntilCalled();
  ASSERT_EQ(2U, consumer.services_.size());

  if (consumer.services_[0].action != ASCIIToUTF16("edit"))
    std::swap(consumer.services_[0],consumer.services_[1]);

  EXPECT_EQ(service, consumer.services_[0]);
  service.action = ASCIIToUTF16("share");
  EXPECT_EQ(service, consumer.services_[1]);
}

#if defined(ENABLE_WEB_INTENTS)
TEST_F(WebDataServiceTest, WebIntentsDefaultsTest) {
  WebIntentsDefaultsConsumer consumer;

  wds_->GetDefaultWebIntentServicesForAction(ASCIIToUTF16("share"), &consumer);
  WaitUntilCalled();
  EXPECT_EQ(0U, consumer.services_.size());

  DefaultWebIntentService default_service;
  default_service.action = ASCIIToUTF16("share");
  default_service.type = ASCIIToUTF16("type");
  default_service.user_date = 1;
  default_service.suppression = 4;
  default_service.service_url = "service_url";
  wds_->AddDefaultWebIntentService(default_service);

  default_service.action = ASCIIToUTF16("share2");
  default_service.service_url = "service_url_2";
  wds_->AddDefaultWebIntentService(default_service);

  wds_->GetDefaultWebIntentServicesForAction(ASCIIToUTF16("share"), &consumer);
  WaitUntilCalled();
  ASSERT_EQ(1U, consumer.services_.size());
  EXPECT_EQ("service_url", consumer.services_[0].service_url);

  wds_->GetAllDefaultWebIntentServices(&consumer);
  WaitUntilCalled();
  EXPECT_EQ(2U, consumer.services_.size());

  default_service.action = ASCIIToUTF16("share");
  wds_->RemoveDefaultWebIntentService(default_service);

  wds_->GetDefaultWebIntentServicesForAction(ASCIIToUTF16("share"), &consumer);
  WaitUntilCalled();
  EXPECT_EQ(0U, consumer.services_.size());

  wds_->GetDefaultWebIntentServicesForAction(ASCIIToUTF16("share2"), &consumer);
  WaitUntilCalled();
  ASSERT_EQ(1U, consumer.services_.size());
  EXPECT_EQ("service_url_2", consumer.services_[0].service_url);

  wds_->GetAllDefaultWebIntentServices(&consumer);
  WaitUntilCalled();
  ASSERT_EQ(1U, consumer.services_.size());
  EXPECT_EQ("service_url_2", consumer.services_[0].service_url);
}
#endif // defined(ENABLE_WEB_INTENTS)

TEST_F(WebDataServiceTest, DidDefaultSearchProviderChangeOnNewProfile) {
  KeywordsConsumer consumer;
  wds_->GetKeywords(&consumer);
  WaitUntilCalled();
  ASSERT_TRUE(consumer.load_succeeded);
  EXPECT_FALSE(consumer.keywords_result.did_default_search_provider_change);
  EXPECT_FALSE(consumer.keywords_result.backup_valid);
}
