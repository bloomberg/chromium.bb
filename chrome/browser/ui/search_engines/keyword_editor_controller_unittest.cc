// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/search_engines/template_url_table_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/table_model_observer.h"

static const string16 kA(ASCIIToUTF16("a"));
static const string16 kA1(ASCIIToUTF16("a1"));
static const string16 kB(ASCIIToUTF16("b"));
static const string16 kB1(ASCIIToUTF16("b1"));

// Base class for keyword editor tests. Creates a profile containing an
// empty TemplateURLService.
class KeywordEditorControllerTest : public testing::Test,
                                    public ui::TableModelObserver {
 public:
  // Initializes all of the state.
  void Init(bool simulate_load_failure);

  virtual void SetUp() {
    Init(false);
  }

  virtual void OnModelChanged() OVERRIDE {
    model_changed_count_++;
  }

  virtual void OnItemsChanged(int start, int length) OVERRIDE {
    items_changed_count_++;
  }

  virtual void OnItemsAdded(int start, int length) OVERRIDE {
    added_count_++;
  }

  virtual void OnItemsRemoved(int start, int length) OVERRIDE {
    removed_count_++;
  }

  void VerifyChangeCount(int model_changed_count, int item_changed_count,
                         int added_count, int removed_count) {
    ASSERT_EQ(model_changed_count, model_changed_count_);
    ASSERT_EQ(item_changed_count, items_changed_count_);
    ASSERT_EQ(added_count, added_count_);
    ASSERT_EQ(removed_count, removed_count_);
    ClearChangeCount();
  }

  void ClearChangeCount() {
    model_changed_count_ = items_changed_count_ = added_count_ =
        removed_count_ = 0;
  }

  void SimulateDefaultSearchIsManaged(const std::string& url) {
    ASSERT_FALSE(url.empty());
    TestingPrefServiceSyncable* service = profile_->GetTestingPrefService();
    service->SetManagedPref(prefs::kDefaultSearchProviderEnabled,
                            new base::FundamentalValue(true));
    service->SetManagedPref(prefs::kDefaultSearchProviderSearchURL,
                            new base::StringValue(url));
    service->SetManagedPref(prefs::kDefaultSearchProviderName,
                            new base::StringValue("managed"));
    // Clear the IDs that are not specified via policy.
    service->SetManagedPref(prefs::kDefaultSearchProviderID,
                            new base::StringValue(std::string()));
    service->SetManagedPref(prefs::kDefaultSearchProviderPrepopulateID,
                            new base::StringValue(std::string()));
    model_->Observe(chrome::NOTIFICATION_DEFAULT_SEARCH_POLICY_CHANGED,
                    content::NotificationService::AllSources(),
                    content::NotificationService::NoDetails());
  }

  TemplateURLTableModel* table_model() const {
    return controller_->table_model();
  }

 protected:
  base::MessageLoopForUI message_loop_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<KeywordEditorController> controller_;
  TemplateURLService* model_;

  int model_changed_count_;
  int items_changed_count_;
  int added_count_;
  int removed_count_;
};

void KeywordEditorControllerTest::Init(bool simulate_load_failure) {
  ClearChangeCount();

  // If init is called twice, make sure that the controller is destroyed before
  // the profile is.
  controller_.reset();
  profile_.reset(new TestingProfile());
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_.get(), &TemplateURLServiceFactory::BuildInstanceFor);

  model_ = TemplateURLServiceFactory::GetForProfile(profile_.get());
  if (simulate_load_failure)
    model_->OnWebDataServiceRequestDone(0, NULL);

  controller_.reset(new KeywordEditorController(profile_.get()));
  controller_->table_model()->SetObserver(this);
}

// Tests adding a TemplateURL.
TEST_F(KeywordEditorControllerTest, Add) {
  controller_->AddTemplateURL(kA, kB, "http://c");

  // Verify the observer was notified.
  VerifyChangeCount(0, 0, 1, 0);
  if (HasFatalFailure())
    return;

  // Verify the TableModel has the new data.
  ASSERT_EQ(1, table_model()->RowCount());

  // Verify the TemplateURLService has the new entry.
  ASSERT_EQ(1U, model_->GetTemplateURLs().size());

  // Verify the entry is what we added.
  const TemplateURL* turl = model_->GetTemplateURLs()[0];
  EXPECT_EQ(ASCIIToUTF16("a"), turl->short_name());
  EXPECT_EQ(ASCIIToUTF16("b"), turl->keyword());
  EXPECT_EQ("http://c", turl->url());
}

// Tests modifying a TemplateURL.
TEST_F(KeywordEditorControllerTest, Modify) {
  controller_->AddTemplateURL(kA, kB, "http://c");
  ClearChangeCount();

  // Modify the entry.
  TemplateURL* turl = model_->GetTemplateURLs()[0];
  controller_->ModifyTemplateURL(turl, kA1, kB1, "http://c1");

  // Make sure it was updated appropriately.
  VerifyChangeCount(0, 1, 0, 0);
  EXPECT_EQ(ASCIIToUTF16("a1"), turl->short_name());
  EXPECT_EQ(ASCIIToUTF16("b1"), turl->keyword());
  EXPECT_EQ("http://c1", turl->url());
}

// Tests making a TemplateURL the default search provider.
TEST_F(KeywordEditorControllerTest, MakeDefault) {
  controller_->AddTemplateURL(kA, kB, "http://c{searchTerms}");
  ClearChangeCount();

  const TemplateURL* turl = model_->GetTemplateURLs()[0];
  int new_default = controller_->MakeDefaultTemplateURL(0);
  EXPECT_EQ(0, new_default);
  // Making an item the default sends a handful of changes. Which are sent isn't
  // important, what is important is 'something' is sent.
  ASSERT_TRUE(items_changed_count_ > 0 || added_count_ > 0 ||
              removed_count_ > 0);
  ASSERT_TRUE(model_->GetDefaultSearchProvider() == turl);

  // Making it default a second time should fail.
  new_default = controller_->MakeDefaultTemplateURL(0);
  EXPECT_EQ(-1, new_default);
}

// Tests that a TemplateURL can't be made the default if the default search
// provider is managed via policy.
TEST_F(KeywordEditorControllerTest, CannotSetDefaultWhileManaged) {
  controller_->AddTemplateURL(kA, kB, "http://c{searchTerms}");
  controller_->AddTemplateURL(kA1, kB1, "http://d{searchTerms}");
  ClearChangeCount();

  const TemplateURL* turl1 =
      model_->GetTemplateURLForKeyword(ASCIIToUTF16("b"));
  ASSERT_TRUE(turl1 != NULL);
  const TemplateURL* turl2 =
      model_->GetTemplateURLForKeyword(ASCIIToUTF16("b1"));
  ASSERT_TRUE(turl2 != NULL);

  EXPECT_TRUE(controller_->CanMakeDefault(turl1));
  EXPECT_TRUE(controller_->CanMakeDefault(turl2));

  SimulateDefaultSearchIsManaged(turl2->url());
  EXPECT_TRUE(model_->is_default_search_managed());

  EXPECT_FALSE(controller_->CanMakeDefault(turl1));
  EXPECT_FALSE(controller_->CanMakeDefault(turl2));
}

// Tests that a TemplateURL can't be edited if it is the managed default search
// provider.
TEST_F(KeywordEditorControllerTest, EditManagedDefault) {
  controller_->AddTemplateURL(kA, kB, "http://c{searchTerms}");
  controller_->AddTemplateURL(kA1, kB1, "http://d{searchTerms}");
  ClearChangeCount();

  const TemplateURL* turl1 =
      model_->GetTemplateURLForKeyword(ASCIIToUTF16("b"));
  ASSERT_TRUE(turl1 != NULL);
  const TemplateURL* turl2 =
      model_->GetTemplateURLForKeyword(ASCIIToUTF16("b1"));
  ASSERT_TRUE(turl2 != NULL);

  EXPECT_TRUE(controller_->CanEdit(turl1));
  EXPECT_TRUE(controller_->CanEdit(turl2));

  // Simulate setting a managed default.  This will add another template URL to
  // the model.
  SimulateDefaultSearchIsManaged(turl2->url());
  EXPECT_TRUE(model_->is_default_search_managed());
  EXPECT_TRUE(controller_->CanEdit(turl1));
  EXPECT_TRUE(controller_->CanEdit(turl2));
  EXPECT_FALSE(controller_->CanEdit(model_->GetDefaultSearchProvider()));
}

TEST_F(KeywordEditorControllerTest, MakeDefaultNoWebData) {
  // Simulate a failure to load Web Data.
  Init(true);

  controller_->AddTemplateURL(kA, kB, "http://c{searchTerms}");
  ClearChangeCount();

  // This should not result in a crash.
  int new_default = controller_->MakeDefaultTemplateURL(0);
  EXPECT_EQ(0, new_default);
}

// Mutates the TemplateURLService and make sure table model is updating
// appropriately.
TEST_F(KeywordEditorControllerTest, MutateTemplateURLService) {
  TemplateURLData data;
  data.short_name = ASCIIToUTF16("b");
  data.SetKeyword(ASCIIToUTF16("a"));
  TemplateURL* turl = new TemplateURL(profile_.get(), data);
  model_->Add(turl);

  // Table model should have updated.
  VerifyChangeCount(1, 0, 0, 0);

  // And should contain the newly added TemplateURL.
  ASSERT_EQ(1, table_model()->RowCount());
  ASSERT_EQ(0, table_model()->IndexOfTemplateURL(turl));
}
