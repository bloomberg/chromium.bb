// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/importer/external_process_importer_host.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/importer/importer_unittest_utils.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/imported_favicon_usage.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(estade): some of these are disabled on mac. http://crbug.com/48007
// TODO(jschuh): Disabled on Win64 build. http://crbug.com/179688
#if defined(OS_MACOSX) || (defined(OS_WIN) && defined(ARCH_CPU_X86_64))
#define MAYBE_IMPORTER(x) DISABLED_##x
#else
#define MAYBE_IMPORTER(x) x
#endif

namespace {

struct PasswordInfo {
  const char* origin;
  const char* action;
  const char* realm;
  const wchar_t* username_element;
  const wchar_t* username;
  const wchar_t* password_element;
  const wchar_t* password;
  bool blacklisted;
};

struct KeywordInfo {
  const wchar_t* keyword;
  const char* url;
};

const BookmarkInfo kFirefoxBookmarks[] = {
  {true, 1, {L"Bookmarks Toolbar"},
    L"Toolbar",
    "http://site/"},
  {false, 0, {},
    L"Title",
    "http://www.google.com/"},
};

const PasswordInfo kFirefoxPasswords[] = {
  {"http://localhost:8080/", "http://localhost:8080/", "http://localhost:8080/",
    L"loginuser", L"abc", L"loginpass", L"123", false},
  {"http://localhost:8080/", "", "http://localhost:8080/localhost",
    L"", L"http", L"", L"Http1+1abcdefg", false},
};

const KeywordInfo kFirefoxKeywords[] = {
  { L"amazon.com",
    "http://www.amazon.com/exec/obidos/external-search/?field-keywords="
    "{searchTerms}&mode=blended" },
  { L"answers.com",
    "http://www.answers.com/main/ntquery?s={searchTerms}&gwp=13" },
  { L"search.creativecommons.org",
    "http://search.creativecommons.org/?q={searchTerms}" },
  { L"search.ebay.com",
    "http://search.ebay.com/search/search.dll?query={searchTerms}&"
    "MfcISAPICommand=GetResult&ht=1&ebaytag1=ebayreg&srchdesc=n&"
    "maxRecordsReturned=300&maxRecordsPerPage=50&SortProperty=MetaEndSort" },
  { L"google.com",
    "http://www.google.com/search?q={searchTerms}&ie=utf-8&oe=utf-8&aq=t" },
  { L"en.wikipedia.org",
    "http://en.wikipedia.org/wiki/Special:Search?search={searchTerms}" },
  { L"search.yahoo.com",
    "http://search.yahoo.com/search?p={searchTerms}&ei=UTF-8" },
  { L"flickr.com",
    "http://www.flickr.com/photos/tags/?q={searchTerms}" },
  { L"imdb.com",
    "http://www.imdb.com/find?q={searchTerms}" },
  { L"webster.com",
    "http://www.webster.com/cgi-bin/dictionary?va={searchTerms}" },
  // Search keywords.
  { L"\x4E2D\x6587", "http://www.google.com/" },
};

class FirefoxObserver : public ProfileWriter,
                        public importer::ImporterProgressObserver {
 public:
  FirefoxObserver()
      : ProfileWriter(NULL), bookmark_count_(0), history_count_(0),
        password_count_(0), keyword_count_(0), import_search_engines_(true) {
  }

  explicit FirefoxObserver(bool import_search_engines)
      : ProfileWriter(NULL), bookmark_count_(0), history_count_(0),
        password_count_(0), keyword_count_(0),
        import_search_engines_(import_search_engines) {
  }

  // importer::ImporterProgressObserver:
  virtual void ImportStarted() OVERRIDE {}
  virtual void ImportItemStarted(importer::ImportItem item) OVERRIDE {}
  virtual void ImportItemEnded(importer::ImportItem item) OVERRIDE {}
  virtual void ImportEnded() OVERRIDE {
    base::MessageLoop::current()->Quit();
    EXPECT_EQ(arraysize(kFirefoxBookmarks), bookmark_count_);
    EXPECT_EQ(1U, history_count_);
    EXPECT_EQ(arraysize(kFirefoxPasswords), password_count_);
    if (import_search_engines_)
      EXPECT_EQ(arraysize(kFirefoxKeywords), keyword_count_);
  }

  virtual bool BookmarkModelIsLoaded() const OVERRIDE {
    // Profile is ready for writing.
    return true;
  }

  virtual bool TemplateURLServiceIsLoaded() const OVERRIDE {
    return true;
  }

  virtual void AddPasswordForm(const autofill::PasswordForm& form) OVERRIDE {
    PasswordInfo p = kFirefoxPasswords[password_count_];
    EXPECT_EQ(p.origin, form.origin.spec());
    EXPECT_EQ(p.realm, form.signon_realm);
    EXPECT_EQ(p.action, form.action.spec());
    EXPECT_EQ(WideToUTF16(p.username_element), form.username_element);
    EXPECT_EQ(WideToUTF16(p.username), form.username_value);
    EXPECT_EQ(WideToUTF16(p.password_element), form.password_element);
    EXPECT_EQ(WideToUTF16(p.password), form.password_value);
    EXPECT_EQ(p.blacklisted, form.blacklisted_by_user);
    ++password_count_;
  }

  virtual void AddHistoryPage(const history::URLRows& page,
                              history::VisitSource visit_source) OVERRIDE {
    ASSERT_EQ(3U, page.size());
    EXPECT_EQ("http://www.google.com/", page[0].url().spec());
    EXPECT_EQ(ASCIIToUTF16("Google"), page[0].title());
    EXPECT_EQ("http://www.google.com/", page[1].url().spec());
    EXPECT_EQ(ASCIIToUTF16("Google"), page[1].title());
    EXPECT_EQ("http://www.cs.unc.edu/~jbs/resources/perl/perl-cgi/programs/"
              "form1-POST.html", page[2].url().spec());
    EXPECT_EQ(ASCIIToUTF16("example form (POST)"), page[2].title());
    EXPECT_EQ(history::SOURCE_FIREFOX_IMPORTED, visit_source);
    ++history_count_;
  }

  virtual void AddBookmarks(const std::vector<ImportedBookmarkEntry>& bookmarks,
                            const string16& top_level_folder_name) OVERRIDE {
    ASSERT_LE(bookmark_count_ + bookmarks.size(), arraysize(kFirefoxBookmarks));
    // Importer should import the FF favorites the same as the list, in the same
    // order.
    for (size_t i = 0; i < bookmarks.size(); ++i) {
      EXPECT_NO_FATAL_FAILURE(
          TestEqualBookmarkEntry(bookmarks[i],
                                 kFirefoxBookmarks[bookmark_count_])) << i;
      ++bookmark_count_;
    }
  }

  virtual void AddKeywords(ScopedVector<TemplateURL> template_urls,
                           bool unique_on_host_and_path) OVERRIDE {
    for (size_t i = 0; i < template_urls.size(); ++i) {
      // The order might not be deterministic, look in the expected list for
      // that template URL.
      bool found = false;
      string16 keyword = template_urls[i]->keyword();
      for (size_t j = 0; j < arraysize(kFirefoxKeywords); ++j) {
        if (template_urls[i]->keyword() ==
                WideToUTF16Hack(kFirefoxKeywords[j].keyword)) {
          EXPECT_EQ(kFirefoxKeywords[j].url, template_urls[i]->url());
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found);
      ++keyword_count_;
    }
  }

  virtual void AddFavicons(
      const std::vector<ImportedFaviconUsage>& favicons) OVERRIDE {
  }

 private:
  virtual ~FirefoxObserver() {}

  size_t bookmark_count_;
  size_t history_count_;
  size_t password_count_;
  size_t keyword_count_;
  bool import_search_engines_;
};

}  // namespace

// These tests need to be browser tests in order to be able to run the OOP
// import (via ExternalProcessImporterHost) which launches a utility process on
// supported platforms.
class FirefoxProfileImporterBrowserTest : public InProcessBrowserTest {
 protected:
  virtual void SetUp() OVERRIDE {
    // Creates a new profile in a new subdirectory in the temp directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath test_path = temp_dir_.path().AppendASCII("ImporterTest");
    base::DeleteFile(test_path, true);
    file_util::CreateDirectory(test_path);
    profile_path_ = test_path.AppendASCII("profile");
    app_path_ = test_path.AppendASCII("app");
    file_util::CreateDirectory(app_path_);

    // This will launch the browser test and thus needs to happen last.
    InProcessBrowserTest::SetUp();
  }

  void Firefox3xImporterBrowserTest(
      std::string profile_dir,
      importer::ImporterProgressObserver* observer,
      ProfileWriter* writer,
      bool import_search_plugins) {
    base::FilePath data_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
    data_path = data_path.AppendASCII(profile_dir);
    ASSERT_TRUE(base::CopyDirectory(data_path, profile_path_, true));
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
    data_path = data_path.AppendASCII("firefox3_nss");
    ASSERT_TRUE(base::CopyDirectory(data_path, profile_path_, false));

    base::FilePath search_engine_path = app_path_;
    search_engine_path = search_engine_path.AppendASCII("searchplugins");
    file_util::CreateDirectory(search_engine_path);
    if (import_search_plugins) {
      ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
      data_path = data_path.AppendASCII("firefox3_searchplugins");
      if (!base::PathExists(data_path)) {
        // TODO(maruel):  Create search test data that we can open source!
        LOG(ERROR) << L"Missing internal test data";
        return;
      }
      ASSERT_TRUE(base::CopyDirectory(data_path, search_engine_path, false));
    }

    importer::SourceProfile source_profile;
    source_profile.importer_type = importer::TYPE_FIREFOX;
    source_profile.app_path = app_path_;
    source_profile.source_path = profile_path_;
    source_profile.locale = "en-US";

    int items = importer::HISTORY | importer::PASSWORDS | importer::FAVORITES;
    if (import_search_plugins)
      items = items | importer::SEARCH_ENGINES;

    // Deletes itself.
    ExternalProcessImporterHost* host = new ExternalProcessImporterHost;
    host->set_observer(observer);
    host->StartImportSettings(source_profile,
                              browser()->profile(),
                              items,
                              make_scoped_refptr(writer).get());
    base::MessageLoop::current()->Run();
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath profile_path_;
  base::FilePath app_path_;
};

IN_PROC_BROWSER_TEST_F(FirefoxProfileImporterBrowserTest,
                       MAYBE_IMPORTER(Firefox30Importer)) {
  scoped_refptr<FirefoxObserver> observer(new FirefoxObserver());
  Firefox3xImporterBrowserTest("firefox3_profile", observer.get(),
                               observer.get(), true);
}

IN_PROC_BROWSER_TEST_F(FirefoxProfileImporterBrowserTest,
                       MAYBE_IMPORTER(Firefox35Importer)) {
  bool import_search_engines = false;
  scoped_refptr<FirefoxObserver> observer(
      new FirefoxObserver(import_search_engines));
  Firefox3xImporterBrowserTest("firefox35_profile", observer.get(),
                               observer.get(), import_search_engines);
}
