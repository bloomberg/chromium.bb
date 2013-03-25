// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_service_test_util.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/value_conversion_util.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"


#if defined(OS_CHROMEOS)
#include "chrome/browser/google/google_util_chromeos.h"
#endif

using content::BrowserThread;

namespace {

// A callback used to coordinate when the database has finished processing
// requests. See note in BlockTillServiceProcessesRequests for details.
//
// Schedules a QuitClosure on the message loop it was created with.
void QuitCallback(MessageLoop* message_loop) {
  message_loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

// Blocks the caller until thread has finished servicing all pending
// requests.
static void WaitForThreadToProcessRequests(BrowserThread::ID identifier) {
  // Schedule a task on the thread that is processed after all
  // pending requests on the thread.
  BrowserThread::PostTask(identifier, FROM_HERE,
                          base::Bind(&QuitCallback, MessageLoop::current()));
  MessageLoop::current()->Run();
}

}  // namespace

// Trivial subclass of TemplateURLService that records the last invocation of
// SetKeywordSearchTermsForURL.
class TestingTemplateURLService : public TemplateURLService {
 public:
  static ProfileKeyedService* Build(Profile* profile) {
    return new TestingTemplateURLService(profile);
  }

  explicit TestingTemplateURLService(Profile* profile)
      : TemplateURLService(profile) {
  }

  string16 GetAndClearSearchTerm() {
    string16 search_term;
    search_term.swap(search_term_);
    return search_term;
  }

 protected:
  virtual void SetKeywordSearchTermsForURL(const TemplateURL* t_url,
                                           const GURL& url,
                                           const string16& term) OVERRIDE {
    search_term_ = term;
  }

 private:
  string16 search_term_;

  DISALLOW_COPY_AND_ASSIGN(TestingTemplateURLService);
};

TemplateURLServiceTestUtil::TemplateURLServiceTestUtil()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      db_thread_(BrowserThread::DB),
      io_thread_(BrowserThread::IO),
      changed_count_(0) {
}

TemplateURLServiceTestUtil::~TemplateURLServiceTestUtil() {
}

void TemplateURLServiceTestUtil::SetUp() {
  // Make unique temp directory.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  profile_.reset(new TestingProfile(temp_dir_.path()));
  db_thread_.Start();
  profile_->CreateWebDataService();

  TemplateURLService* service = static_cast<TemplateURLService*>(
      TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile_.get(), TestingTemplateURLService::Build));
  service->AddObserver(this);

#if defined(OS_CHROMEOS)
  google_util::chromeos::ClearBrandForCurrentSession();
#endif
}

void TemplateURLServiceTestUtil::TearDown() {
  if (profile_.get()) {
    // Clear the request context so it will get deleted. This should be done
    // before shutting down the I/O thread to avoid memory leaks.
    profile_->ResetRequestContext();
    profile_.reset();
  }

  // Wait for the delete of the request context to happen.
  if (io_thread_.IsRunning())
    TemplateURLServiceTestUtil::BlockTillIOThreadProcessesRequests();

  // The I/O thread must be shutdown before the DB thread.
  io_thread_.Stop();

  // Note that we must ensure the DB thread is stopped after WDS
  // shutdown (so it can commit pending transactions) but before
  // deleting the test profile directory, otherwise we may not be
  // able to delete it due to an open transaction.
  // Schedule another task on the DB thread to notify us that it's safe to
  // carry on with the test.
  base::WaitableEvent done(false, false);
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done)));
  done.Wait();
  MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  MessageLoop::current()->Run();
  db_thread_.Stop();

  UIThreadSearchTermsData::SetGoogleBaseURL(std::string());

  // Flush the message loop to make application verifiers happy.
  message_loop_.RunUntilIdle();
}

void TemplateURLServiceTestUtil::OnTemplateURLServiceChanged() {
  changed_count_++;
}

int TemplateURLServiceTestUtil::GetObserverCount() {
  return changed_count_;
}

void TemplateURLServiceTestUtil::ResetObserverCount() {
  changed_count_ = 0;
}

void TemplateURLServiceTestUtil::BlockTillServiceProcessesRequests() {
  WaitForThreadToProcessRequests(BrowserThread::DB);
}

void TemplateURLServiceTestUtil::BlockTillIOThreadProcessesRequests() {
  WaitForThreadToProcessRequests(BrowserThread::IO);
}

void TemplateURLServiceTestUtil::VerifyLoad() {
  ASSERT_FALSE(model()->loaded());
  model()->Load();
  BlockTillServiceProcessesRequests();
  EXPECT_EQ(1, GetObserverCount());
  ResetObserverCount();
}

void TemplateURLServiceTestUtil::ChangeModelToLoadState() {
  model()->ChangeToLoadedState();
  // Initialize the web data service so that the database gets updated with
  // any changes made.

  model()->service_ = WebDataService::FromBrowserContext(profile_.get());
  BlockTillServiceProcessesRequests();
}

void TemplateURLServiceTestUtil::ClearModel() {
  TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), NULL);
}

void TemplateURLServiceTestUtil::ResetModel(bool verify_load) {
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_.get(), TestingTemplateURLService::Build);
  model()->AddObserver(this);
  changed_count_ = 0;
  if (verify_load)
    VerifyLoad();
}

string16 TemplateURLServiceTestUtil::GetAndClearSearchTerm() {
  return
      static_cast<TestingTemplateURLService*>(model())->GetAndClearSearchTerm();
}

void TemplateURLServiceTestUtil::SetGoogleBaseURL(const GURL& base_url) const {
  DCHECK(base_url.is_valid());
  UIThreadSearchTermsData data(profile_.get());
  GoogleURLTracker::UpdatedDetails urls(GURL(data.GoogleBaseURLValue()),
                                        base_url);
  UIThreadSearchTermsData::SetGoogleBaseURL(base_url.spec());
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
      content::Source<Profile>(profile_.get()),
      content::Details<GoogleURLTracker::UpdatedDetails>(&urls));
}

void TemplateURLServiceTestUtil::SetManagedDefaultSearchPreferences(
    bool enabled,
    const std::string& name,
    const std::string& keyword,
    const std::string& search_url,
    const std::string& suggest_url,
    const std::string& icon_url,
    const std::string& encodings,
    const std::string& alternate_url,
    const std::string& search_terms_replacement_key) {
  TestingPrefServiceSyncable* pref_service = profile_->GetTestingPrefService();
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderEnabled,
                               Value::CreateBooleanValue(enabled));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderName,
                               Value::CreateStringValue(name));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderKeyword,
                               Value::CreateStringValue(keyword));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderSearchURL,
                               Value::CreateStringValue(search_url));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderSuggestURL,
                               Value::CreateStringValue(suggest_url));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderIconURL,
                               Value::CreateStringValue(icon_url));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderEncodings,
                               Value::CreateStringValue(encodings));
  pref_service->SetManagedPref(prefs::kDefaultSearchProviderAlternateURLs,
      alternate_url.empty() ? new base::ListValue() :
          CreateListValueFrom(alternate_url));
  pref_service->SetManagedPref(
      prefs::kDefaultSearchProviderSearchTermsReplacementKey,
      Value::CreateStringValue(search_terms_replacement_key));
  model()->Observe(chrome::NOTIFICATION_DEFAULT_SEARCH_POLICY_CHANGED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());
}

void TemplateURLServiceTestUtil::RemoveManagedDefaultSearchPreferences() {
  TestingPrefServiceSyncable* pref_service = profile_->GetTestingPrefService();
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderEnabled);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderName);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderKeyword);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderSearchURL);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderSuggestURL);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderIconURL);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderEncodings);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderAlternateURLs);
  pref_service->RemoveManagedPref(
      prefs::kDefaultSearchProviderSearchTermsReplacementKey);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderID);
  pref_service->RemoveManagedPref(prefs::kDefaultSearchProviderPrepopulateID);
  model()->Observe(chrome::NOTIFICATION_DEFAULT_SEARCH_POLICY_CHANGED,
                   content::NotificationService::AllSources(),
                   content::NotificationService::NoDetails());
}

TemplateURLService* TemplateURLServiceTestUtil::model() const {
  return TemplateURLServiceFactory::GetForProfile(profile_.get());
}

TestingProfile* TemplateURLServiceTestUtil::profile() const {
  return profile_.get();
}

void TemplateURLServiceTestUtil::StartIOThread() {
  io_thread_.StartIOThread();
}

void TemplateURLServiceTestUtil::PumpLoop() {
  message_loop_.RunUntilIdle();
}
