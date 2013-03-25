// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/history_contents_provider.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

using content::BrowserThread;

namespace {

struct TestEntry {
  const char* url;
  const char* title;
  const char* body;
} test_entries[] = {
  {"http://www.google.com/1", "PAGEONE 1",   "FOO some body text"},
  {"http://www.google.com/2", "PAGEONE 2",   "FOO some more blah blah"},
  {"http://www.google.com/3", "PAGETHREE 3", "BAR some hello world for you"},
  {"http://testsearch.com/?q=thequery", "Search results", "MOO search results"},
  {"http://testsearch.com/", "Search engine", "MOO best engine ever"},
};

class HistoryContentsProviderTest : public testing::Test,
                                    public AutocompleteProviderListener {
 public:
  HistoryContentsProviderTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) {}

  void RunQuery(const AutocompleteInput& input,
                bool minimal_changes) {
    provider_->Start(input, minimal_changes);

    // When we're waiting for asynchronous messages, we have to spin the message
    // loop. This will be exited in the OnProviderUpdate function when complete.
    if (input.matches_requested() == AutocompleteInput::ALL_MATCHES)
      MessageLoop::current()->Run();
  }

  const ACMatches& matches() const { return provider_->matches(); }
  TestingProfile* profile() const { return profile_.get(); }
  HistoryContentsProvider* provider() const { return provider_.get(); }

 protected:
  virtual bool BodyOnly() { return false; }

 private:
  static ProfileKeyedService* CreateTemplateURLService(Profile* profile) {
    return new TemplateURLService(profile);
  }

  // testing::Test
  virtual void SetUp() {
    profile_.reset(new TestingProfile());
    profile_->CreateHistoryService(false, false);

    HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(profile_.get(),
                                             Profile::EXPLICIT_ACCESS);

    // Populate history.
    for (size_t i = 0; i < arraysize(test_entries); i++) {
      // We need the ID scope and page ID so that the visit tracker can find it.
      // We just use the index for the page ID below.
      const void* id_scope = reinterpret_cast<void*>(1);
      GURL url(test_entries[i].url);

      // Add everything in order of time. We don't want to have a time that
      // is "right now" or it will nondeterministically appear in the results.
      Time t = Time::Now() - TimeDelta::FromDays(arraysize(test_entries) + i);

      history_service->AddPage(url, t, id_scope, i, GURL(),
                               history::RedirectList(),
                               content::PAGE_TRANSITION_LINK,
                               history::SOURCE_BROWSED, false);
      history_service->SetPageTitle(url, UTF8ToUTF16(test_entries[i].title));
      history_service->SetPageContents(url, UTF8ToUTF16(test_entries[i].body));
    }

    provider_ = new HistoryContentsProvider(this, profile_.get(), BodyOnly());

    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &HistoryContentsProviderTest::CreateTemplateURLService);
  }

  virtual void TearDown() {
    provider_ = NULL;
    profile_.reset(NULL);
  }

  // AutocompleteProviderListener:
  virtual void OnProviderUpdate(bool updated_matches) OVERRIDE {
    // We must quit the message loop (if running) to return control to the test.
    // Note, calling Quit() directly will checkfail if the loop isn't running,
    // so we post a task, which is safe for either case.
    MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<HistoryContentsProvider> provider_;
};

class HistoryContentsProviderBodyOnlyTest : public HistoryContentsProviderTest {
 protected:
  virtual bool BodyOnly() OVERRIDE { return true; }
};

TEST_F(HistoryContentsProviderTest, Body) {
  AutocompleteInput input(ASCIIToUTF16("FOO"), string16::npos, string16(),
                          GURL(), true, false, true,
                          AutocompleteInput::ALL_MATCHES);
  RunQuery(input, false);

  // The results should be the first two pages, in decreasing order.
  const ACMatches& m = matches();
  ASSERT_EQ(2U, m.size());
  EXPECT_EQ(test_entries[0].url, m[0].destination_url.spec());
  EXPECT_STREQ(test_entries[0].title, UTF16ToUTF8(m[0].description).c_str());
  EXPECT_EQ(test_entries[1].url, m[1].destination_url.spec());
  EXPECT_STREQ(test_entries[1].title, UTF16ToUTF8(m[1].description).c_str());
}

TEST_F(HistoryContentsProviderTest, Title) {
  AutocompleteInput input(ASCIIToUTF16("PAGEONE"), string16::npos, string16(),
                          GURL(), true, false, true,
                          AutocompleteInput::ALL_MATCHES);
  RunQuery(input, false);

  // The results should be the first two pages.
  const ACMatches& m = matches();
  ASSERT_EQ(2U, m.size());
  EXPECT_EQ(test_entries[0].url, m[0].destination_url.spec());
  EXPECT_STREQ(test_entries[0].title, UTF16ToUTF8(m[0].description).c_str());
  EXPECT_EQ(test_entries[1].url, m[1].destination_url.spec());
  EXPECT_STREQ(test_entries[1].title, UTF16ToUTF8(m[1].description).c_str());
}

// The "minimal changes" flag should mean that we don't re-query the DB.
TEST_F(HistoryContentsProviderTest, MinimalChanges) {
  // A minimal changes request when there have been no real queries should
  // give us no results.
  AutocompleteInput sync_input(ASCIIToUTF16("PAGEONE"), string16::npos,
                               string16(), GURL(), true, false, true,
                               AutocompleteInput::SYNCHRONOUS_MATCHES);
  RunQuery(sync_input, true);
  const ACMatches& m1 = matches();
  EXPECT_EQ(0U, m1.size());

  // Now do a "regular" query to get the results.
  AutocompleteInput async_input(ASCIIToUTF16("PAGEONE"), string16::npos,
                                string16(), GURL(), true, false, true,
                                AutocompleteInput::ALL_MATCHES);
  RunQuery(async_input, false);
  const ACMatches& m2 = matches();
  EXPECT_EQ(2U, m2.size());

  // Now do a minimal one where we want synchronous results, and the results
  // should still be there.
  RunQuery(sync_input, true);
  const ACMatches& m3 = matches();
  EXPECT_EQ(2U, m3.size());
}

TEST_F(HistoryContentsProviderBodyOnlyTest, MinimalChanges) {
  // A minimal changes request when there have been no real queries should
  // give us no results.
  AutocompleteInput sync_input(ASCIIToUTF16("PAGEONE"), string16::npos,
                               string16(), GURL(), true, false, true,
                               AutocompleteInput::SYNCHRONOUS_MATCHES);
  RunQuery(sync_input, true);
  const ACMatches& m1 = matches();
  EXPECT_EQ(0U, m1.size());

  // Now do a "regular" query to get no results because we are body-only.
  AutocompleteInput async_input(ASCIIToUTF16("PAGEONE"), string16::npos,
                                string16(), GURL(), true, false, true,
                                AutocompleteInput::ALL_MATCHES);
  RunQuery(async_input, false);
  const ACMatches& m2 = matches();
  EXPECT_EQ(0U, m2.size());

  // Now do a minimal one where we want synchronous results, and the results
  // should still not be there.
  RunQuery(sync_input, true);
  const ACMatches& m3 = matches();
  EXPECT_EQ(0U, m3.size());
}

// Tests that history is deleted properly.
TEST_F(HistoryContentsProviderTest, DeleteMatch) {
  AutocompleteInput input(ASCIIToUTF16("bar"), string16::npos, string16(),
                          GURL(), true, false, true,
                          AutocompleteInput::ALL_MATCHES);
  RunQuery(input, false);

  // Query; the result should be the third page.
  const ACMatches& m = matches();
  ASSERT_EQ(1U, m.size());
  EXPECT_EQ(test_entries[2].url, m[0].destination_url.spec());

  // Now delete the match and ensure it was removed.
  provider()->DeleteMatch(m[0]);
  EXPECT_EQ(0U, matches().size());
}

// Tests deleting starred results from history, not affecting bookmarks/matches.
TEST_F(HistoryContentsProviderTest, DeleteStarredMatch) {
  profile()->CreateBookmarkModel(false);

  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  ui_test_utils::WaitForBookmarkModelToLoad(model);

  // Bookmark a history item.
  GURL bookmark_url(test_entries[2].url);
  bookmark_utils::AddIfNotBookmarked(model, bookmark_url, ASCIIToUTF16("bar"));

  // Get the match to delete its history
  AutocompleteInput input(ASCIIToUTF16("bar"), string16::npos, string16(),
                          GURL(), true, false, true,
                          AutocompleteInput::ALL_MATCHES);
  RunQuery(input, false);
  const ACMatches& m = matches();
  ASSERT_EQ(1U, m.size());

  // Now delete the match and ensure it was *not* removed.
  provider()->DeleteMatch(m[0]);
  EXPECT_EQ(1U, matches().size());

  // Run a query that would only match history (but the history is deleted)
  AutocompleteInput you_input(ASCIIToUTF16("you"), string16::npos, string16(),
                              GURL(), true, false, true,
                              AutocompleteInput::ALL_MATCHES);
  RunQuery(you_input, false);
  EXPECT_EQ(0U, matches().size());
}

TEST_F(HistoryContentsProviderTest, CullSearchResults) {
  // Set up a default search engine.
  TemplateURLData data;
  data.SetKeyword(ASCIIToUTF16("TestEngine"));
  data.SetURL("http://testsearch.com/?q={searchTerms}");
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  TemplateURL* template_url = new TemplateURL(profile(), data);
  template_url_service->Add(template_url);
  template_url_service->SetDefaultSearchProvider(template_url);
  template_url_service->Load();

  AutocompleteInput input(ASCIIToUTF16("moo"), string16::npos, string16(),
                          GURL(), true, false, true,
                          AutocompleteInput::ALL_MATCHES);
  RunQuery(input, false);

  // Run a query that is found on a search result page and on the main page
  // of the default search engine. The result should be the main page, the
  // SRP should be culled.
  const ACMatches& m = matches();
  ASSERT_EQ(1U, m.size());
  EXPECT_EQ(test_entries[4].url, m[0].destination_url.spec());
}

}  // namespace
