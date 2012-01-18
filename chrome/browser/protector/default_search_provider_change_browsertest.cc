// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/protector/mock_protector.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace protector {

namespace {

// Keyword names and URLs used for testing.

const string16 example_info = ASCIIToUTF16("Info");
const string16 example_info_long = ASCIIToUTF16("Example.info");
const std::string http_example_info = "http://example.info/%s";
const string16 example_com = ASCIIToUTF16("Com");
const string16 example_com_long = ASCIIToUTF16("Example.com");
const std::string http_example_com = "http://example.com/%s";
const string16 example_net = ASCIIToUTF16("Net");
const std::string http_example_net = "http://example.net/%s";

};

class DefaultSearchProviderChangeTest : public InProcessBrowserTest {
 public:
  virtual void SetUpOnMainThread() {
    mock_protector_.reset(new MockProtector(browser()->profile()));

    // Ensure that TemplateURLService is loaded.
    turl_service_ =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ui_test_utils::WaitForTemplateURLServiceToLoad(turl_service_);

    prepopulated_url_.reset(
        TemplateURLPrepopulateData::GetPrepopulatedDefaultSearch(NULL));
  }

  TemplateURL* MakeTemplateURL(const string16& short_name,
                               const string16& keyword,
                               const std::string& search_url) {
    TemplateURL* url = new TemplateURL;
    url->set_short_name(short_name);
    if (keyword.empty())
      url->set_autogenerate_keyword(true);
    else
      url->set_keyword(keyword);
    url->SetURL(search_url, 0, 0);
    return url;
  }

  const TemplateURL* FindTemplateURL(const std::string& search_url) {
    TemplateURLService::TemplateURLVector urls =
        turl_service_->GetTemplateURLs();
    for (TemplateURLService::TemplateURLVector::const_iterator
         it = urls.begin(); it != urls.end(); ++it) {
      if ((*it)->url()->url() == search_url)
        return *it;
    }
    return NULL;
  }

  // Adds a copy of |turl| that will be owned by TemplateURLService.
  void AddCopy(TemplateURL* turl) {
    TemplateURL* turl_copy = new TemplateURL(*turl);
    turl_service_->Add(turl_copy);
  }

  void AddAndSetDefault(TemplateURL* t_url) {
    turl_service_->Add(t_url);
    turl_service_->SetDefaultSearchProvider(t_url);
  }

  string16 GetBubbleMessage(const string16& short_name = string16()) {
    return short_name.empty() ?
        l10n_util::GetStringUTF16(IDS_SEARCH_ENGINE_CHANGE_MESSAGE) :
        l10n_util::GetStringFUTF16(IDS_SEARCH_ENGINE_CHANGE_NO_BACKUP_MESSAGE,
                                   short_name);
  }

  string16 GetChangeSearchButtonText(const string16& short_name = string16()) {
    return short_name.empty() ?
        l10n_util::GetStringUTF16(IDS_CHANGE_SEARCH_ENGINE_NO_NAME) :
        l10n_util::GetStringFUTF16(IDS_CHANGE_SEARCH_ENGINE, short_name);
  }

  string16 GetKeepSearchButtonText(const string16& short_name = string16()) {
    return short_name.empty() ?
        l10n_util::GetStringUTF16(IDS_KEEP_SETTING) :
        l10n_util::GetStringFUTF16(IDS_KEEP_SEARCH_ENGINE, short_name);
  }

  string16 GetOpenSettingsButtonText() {
    return l10n_util::GetStringUTF16(IDS_SELECT_SEARCH_ENGINE);
  }

  void ExpectSettingsOpened(const std::string& subpage) {
    GURL settings_url(chrome::kChromeUISettingsURL + subpage);
    EXPECT_CALL(*mock_protector_.get(), OpenTab(settings_url));
  }

 protected:
  scoped_ptr<MockProtector> mock_protector_;
  TemplateURLService* turl_service_;
  scoped_ptr<TemplateURL> prepopulated_url_;
};

// Tests below call both Apply and Discard on a single change instance.
// This is test-only and should not be happening in real code.

// |backup_url| in further test cases is owned by DefaultSearchProviderChange
// instance, while other TemplateURLs are owned by TemplateURLService.

IN_PROC_BROWSER_TEST_F(DefaultSearchProviderChangeTest, BackupValid) {
  // Most common case: current default search provider exists, backup is valid,
  // they are different.
  TemplateURL* backup_url =
      MakeTemplateURL(example_info, ASCIIToUTF16("a"), http_example_info);
  TemplateURL* current_url =
      MakeTemplateURL(example_com, ASCIIToUTF16("b"), http_example_com);

  AddCopy(backup_url);
  AddAndSetDefault(current_url);

  scoped_ptr<BaseSettingChange> change(
      CreateDefaultSearchProviderChange(current_url, backup_url));
  ASSERT_TRUE(change.get());
  ASSERT_TRUE(change->Init(mock_protector_.get()));

  // Verify that backup is active.
  EXPECT_EQ(FindTemplateURL(http_example_info),
            turl_service_->GetDefaultSearchProvider());

  // Verify text messages.
  EXPECT_EQ(GetBubbleMessage(), change->GetBubbleMessage());
  EXPECT_EQ(GetChangeSearchButtonText(example_com),
            change->GetApplyButtonText());
  EXPECT_EQ(GetKeepSearchButtonText(example_info),
            change->GetDiscardButtonText());

  // Discard does nothing - backup was already active.
  change->Discard();
  EXPECT_EQ(FindTemplateURL(http_example_info),
            turl_service_->GetDefaultSearchProvider());

  // Verify that Apply switches back to |current_url|.
  change->Apply();
  EXPECT_EQ(FindTemplateURL(http_example_com),
            turl_service_->GetDefaultSearchProvider());
}

IN_PROC_BROWSER_TEST_F(DefaultSearchProviderChangeTest, BackupValidLongNames) {
  // Verify that search provider names that are too long are not displayed.
  TemplateURL* backup_url =
      MakeTemplateURL(example_info, ASCIIToUTF16("a"), http_example_info);
  TemplateURL* backup_url_long =
      MakeTemplateURL(example_info_long, ASCIIToUTF16("a"), http_example_info);
  TemplateURL* current_url =
      MakeTemplateURL(example_com, ASCIIToUTF16("b"), http_example_com);
  TemplateURL* current_url_long =
      MakeTemplateURL(example_com_long, ASCIIToUTF16("b"), http_example_com);

  {
    // Backup name too long.
    AddCopy(backup_url_long);
    AddAndSetDefault(current_url);

    scoped_ptr<BaseSettingChange> change(
        CreateDefaultSearchProviderChange(current_url, backup_url_long));
    ASSERT_TRUE(change.get());
    ASSERT_TRUE(change->Init(mock_protector_.get()));

    // Verify text messages.
    EXPECT_EQ(GetBubbleMessage(), change->GetBubbleMessage());
    EXPECT_EQ(GetChangeSearchButtonText(example_com),
              change->GetApplyButtonText());
    EXPECT_EQ(GetKeepSearchButtonText(), change->GetDiscardButtonText());
  }

  {
    // Current name too long.
    AddCopy(backup_url);
    AddAndSetDefault(current_url_long);

    scoped_ptr<BaseSettingChange> change(
        CreateDefaultSearchProviderChange(current_url_long, backup_url));
    ASSERT_TRUE(change.get());
    ASSERT_TRUE(change->Init(mock_protector_.get()));

    // Verify text messages.
    EXPECT_EQ(GetBubbleMessage(), change->GetBubbleMessage());
    EXPECT_EQ(GetChangeSearchButtonText(), change->GetApplyButtonText());
    EXPECT_EQ(GetKeepSearchButtonText(example_info),
              change->GetDiscardButtonText());
  }
}

IN_PROC_BROWSER_TEST_F(DefaultSearchProviderChangeTest, BackupInvalid) {
  // Backup is invalid, current search provider exists, fallback to the
  // prepopulated default search, which exists among keywords.
  TemplateURL* current_url =
      MakeTemplateURL(example_com, ASCIIToUTF16("b"), http_example_com);

  AddAndSetDefault(current_url);

  // Prepopulated default search must exist.
  ASSERT_TRUE(FindTemplateURL(prepopulated_url_->url()->url()));

  scoped_ptr<BaseSettingChange> change(
      CreateDefaultSearchProviderChange(current_url, NULL));
  ASSERT_TRUE(change.get());
  ASSERT_TRUE(change->Init(mock_protector_.get()));

  // Verify that the prepopulated default search is active.
  EXPECT_EQ(FindTemplateURL(prepopulated_url_->url()->url()),
            turl_service_->GetDefaultSearchProvider());

  // Verify text messages.
  EXPECT_EQ(GetBubbleMessage(prepopulated_url_->short_name()),
            change->GetBubbleMessage());
  EXPECT_EQ(GetChangeSearchButtonText(example_com),
            change->GetApplyButtonText());
  EXPECT_EQ(GetOpenSettingsButtonText(), change->GetDiscardButtonText());

  // Verify that search engine settings are opened by Discard.
  ExpectSettingsOpened(chrome::kSearchEnginesSubPage);
  change->Discard();
  EXPECT_EQ(FindTemplateURL(prepopulated_url_->url()->url()),
            turl_service_->GetDefaultSearchProvider());

  // Verify that Apply switches back to |current_url|.
  change->Apply();
  EXPECT_EQ(FindTemplateURL(http_example_com),
            turl_service_->GetDefaultSearchProvider());
}

IN_PROC_BROWSER_TEST_F(DefaultSearchProviderChangeTest,
                       BackupInvalidFallbackRemoved) {
  // Backup is invalid, current search provider exists, fallback to the
  // prepopulated default search, which was removed from keywords (will be
  // added).
  TemplateURL* current_url =
      MakeTemplateURL(example_com, ASCIIToUTF16("b"), http_example_com);

  AddAndSetDefault(current_url);

  const TemplateURL* prepopulated_default =
      FindTemplateURL(prepopulated_url_->url()->url());
  // Prepopulated default search must exist, remove it.
  ASSERT_TRUE(prepopulated_default);
  turl_service_->Remove(prepopulated_default);

  scoped_ptr<BaseSettingChange> change(
      CreateDefaultSearchProviderChange(current_url, NULL));
  ASSERT_TRUE(change.get());
  ASSERT_TRUE(change->Init(mock_protector_.get()));

  // Verify that the prepopulated default search is active.
  EXPECT_EQ(FindTemplateURL(prepopulated_url_->url()->url()),
            turl_service_->GetDefaultSearchProvider());

  // Verify text messages.
  EXPECT_EQ(GetBubbleMessage(prepopulated_url_->short_name()),
            change->GetBubbleMessage());
  EXPECT_EQ(GetChangeSearchButtonText(example_com),
            change->GetApplyButtonText());
  EXPECT_EQ(GetOpenSettingsButtonText(), change->GetDiscardButtonText());

  // Verify that search engine settings are opened by Discard.
  ExpectSettingsOpened(chrome::kSearchEnginesSubPage);
  change->Discard();
  EXPECT_EQ(FindTemplateURL(prepopulated_url_->url()->url()),
            turl_service_->GetDefaultSearchProvider());

  // Verify that Apply switches back to |current_url|.
  change->Apply();
  EXPECT_EQ(FindTemplateURL(http_example_com),
            turl_service_->GetDefaultSearchProvider());
}

IN_PROC_BROWSER_TEST_F(DefaultSearchProviderChangeTest,
                       BackupValidCurrentRemoved) {
  // Backup is valid, no current search provider.
  TemplateURL* backup_url =
      MakeTemplateURL(example_info, ASCIIToUTF16("a"), http_example_info);

  AddCopy(backup_url);
  turl_service_->SetDefaultSearchProvider(NULL);

  scoped_ptr<BaseSettingChange> change(
      CreateDefaultSearchProviderChange(NULL, backup_url));
  ASSERT_TRUE(change.get());
  ASSERT_TRUE(change->Init(mock_protector_.get()));

  // Verify that backup is active.
  EXPECT_EQ(FindTemplateURL(http_example_info),
            turl_service_->GetDefaultSearchProvider());

  // Verify text messages.
  EXPECT_EQ(GetBubbleMessage(), change->GetBubbleMessage());
  EXPECT_EQ(GetOpenSettingsButtonText(), change->GetApplyButtonText());
  EXPECT_EQ(GetKeepSearchButtonText(example_info),
            change->GetDiscardButtonText());

  // Discard does nothing - backup was already active.
  change->Discard();
  EXPECT_EQ(FindTemplateURL(http_example_info),
            turl_service_->GetDefaultSearchProvider());

   // Verify that search engine settings are opened by Apply (no other changes).
  ExpectSettingsOpened(chrome::kSearchEnginesSubPage);
  change->Apply();
  EXPECT_EQ(FindTemplateURL(http_example_info),
            turl_service_->GetDefaultSearchProvider());
}

IN_PROC_BROWSER_TEST_F(DefaultSearchProviderChangeTest,
                       BackupInvalidCurrentRemoved) {
  // Backup is invalid, no current search provider, fallback to the prepopulated
  // default search.
  turl_service_->SetDefaultSearchProvider(NULL);

  scoped_ptr<BaseSettingChange> change(
      CreateDefaultSearchProviderChange(NULL, NULL));
  ASSERT_TRUE(change.get());
  ASSERT_TRUE(change->Init(mock_protector_.get()));

  // Verify that the prepopulated default search is active.
  EXPECT_EQ(FindTemplateURL(prepopulated_url_->url()->url()),
            turl_service_->GetDefaultSearchProvider());

  // Verify text messages.
  EXPECT_EQ(GetBubbleMessage(prepopulated_url_->short_name()),
            change->GetBubbleMessage());
  EXPECT_EQ(string16(), change->GetApplyButtonText());
  EXPECT_EQ(GetOpenSettingsButtonText(), change->GetDiscardButtonText());

  // Verify that search engine settings are opened by Discard.
  ExpectSettingsOpened(chrome::kSearchEnginesSubPage);
  change->Discard();
  EXPECT_EQ(FindTemplateURL(prepopulated_url_->url()->url()),
            turl_service_->GetDefaultSearchProvider());
}

IN_PROC_BROWSER_TEST_F(DefaultSearchProviderChangeTest,
                       BackupInvalidFallbackSameAsCurrent) {
  // Backup is invalid, fallback to the prepopulated default search which is
  // same as the current search provider.
  const TemplateURL* current_url = turl_service_->GetDefaultSearchProvider();

  // Verify that current search provider is same as the prepopulated default.
  ASSERT_TRUE(current_url);
  ASSERT_EQ(current_url, FindTemplateURL(prepopulated_url_->url()->url()));

  scoped_ptr<BaseSettingChange> change(
      CreateDefaultSearchProviderChange(current_url, NULL));
  ASSERT_TRUE(change.get());
  ASSERT_TRUE(change->Init(mock_protector_.get()));

  // Verify that the default search has not changed.
  EXPECT_EQ(current_url, turl_service_->GetDefaultSearchProvider());

  // Verify text messages.
  EXPECT_EQ(GetBubbleMessage(prepopulated_url_->short_name()),
            change->GetBubbleMessage());
  EXPECT_EQ(string16(), change->GetApplyButtonText());
  EXPECT_EQ(GetOpenSettingsButtonText(), change->GetDiscardButtonText());

  // Verify that search engine settings are opened by Discard.
  ExpectSettingsOpened(chrome::kSearchEnginesSubPage);
  change->Discard();
  EXPECT_EQ(current_url, turl_service_->GetDefaultSearchProvider());
}


IN_PROC_BROWSER_TEST_F(DefaultSearchProviderChangeTest,
                       DefaultSearchProviderChangedByUser) {
  // Default search provider is changed by user while the error is active.
  // Setup is the same as in BackupValid test case.
  TemplateURL* backup_url =
      MakeTemplateURL(example_info, ASCIIToUTF16("a"), http_example_info);
  TemplateURL* current_url =
      MakeTemplateURL(example_com, ASCIIToUTF16("b"), http_example_com);
  TemplateURL* new_url =
      MakeTemplateURL(example_net, ASCIIToUTF16("c"), http_example_net);

  AddCopy(backup_url);
  AddAndSetDefault(current_url);

  scoped_ptr<BaseSettingChange> change(
      CreateDefaultSearchProviderChange(current_url, backup_url));
  ASSERT_TRUE(change.get());
  ASSERT_TRUE(change->Init(mock_protector_.get()));

  // Verify that backup is active.
  EXPECT_EQ(FindTemplateURL(http_example_info),
            turl_service_->GetDefaultSearchProvider());

  // Verify that changing search provider externally dismissed the change.
  EXPECT_CALL(*mock_protector_.get(), DismissChange());
  AddAndSetDefault(new_url);
}

IN_PROC_BROWSER_TEST_F(DefaultSearchProviderChangeTest,
                       CurrentSearchProviderRemovedByUser) {
  // Current search provider is removed by user while the error is active.
  // Setup is the same as in BackupValid test case.
  TemplateURL* backup_url =
      MakeTemplateURL(example_info, ASCIIToUTF16("a"), http_example_info);
  TemplateURL* current_url =
      MakeTemplateURL(example_com, ASCIIToUTF16("b"), http_example_com);

  AddCopy(backup_url);
  AddAndSetDefault(current_url);

  scoped_ptr<BaseSettingChange> change(
      CreateDefaultSearchProviderChange(current_url, backup_url));
  ASSERT_TRUE(change.get());
  ASSERT_TRUE(change->Init(mock_protector_.get()));

  // Verify that backup is active.
  EXPECT_EQ(FindTemplateURL(http_example_info),
            turl_service_->GetDefaultSearchProvider());

  turl_service_->Remove(current_url);

  // Verify that text messages altered after removing |current_url|.
  EXPECT_EQ(GetBubbleMessage(), change->GetBubbleMessage());
  EXPECT_EQ(GetOpenSettingsButtonText(), change->GetApplyButtonText());
  EXPECT_EQ(GetKeepSearchButtonText(example_info),
            change->GetDiscardButtonText());

  // Verify that search engine settings are opened by Apply.
  ExpectSettingsOpened(chrome::kSearchEnginesSubPage);
  change->Apply();
  EXPECT_EQ(FindTemplateURL(http_example_info),
            turl_service_->GetDefaultSearchProvider());

  // Discard does nothing - backup was already active.
  change->Discard();
  EXPECT_EQ(FindTemplateURL(http_example_info),
            turl_service_->GetDefaultSearchProvider());
}

}  // namespace protector
