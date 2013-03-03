// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/firefox2_importer.h"
#include "chrome/browser/importer/firefox_importer_unittest_utils.h"
#include "chrome/browser/importer/firefox_importer_utils.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/importer/importer_unittest_utils.h"
#include "chrome/browser/importer/nss_decryptor.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/password_form.h"

// TODO(estade): some of these are disabled on mac. http://crbug.com/48007
// TODO(jschuh): Disabled on Win64 build. http://crbug.com/179688
#if defined(OS_MACOSX) || (defined(OS_WIN) && defined(ARCH_CPU_X86_64))
#define MAYBE_IMPORTER(x) DISABLED_##x
#else
#define MAYBE_IMPORTER(x) x
#endif

// TODO(jschuh): Disabled on Win64 build. http://crbug.com/179688
#if defined(OS_WIN) && defined(ARCH_CPU_X86_64)
#define MAYBE_NSS(x) DISABLED_##x
#else
#define MAYBE_NSS(x) x
#endif

namespace {

const BookmarkInfo kFirefox2Bookmarks[] = {
  {true, 2, {L"Bookmarks Toolbar Folder", L"Folder"},
   L"On Toolbar's Subfolder",
   "http://on.toolbar/bookmark/folder"},
  {true, 1, {L"Bookmarks Toolbar Folder"},
   L"On Bookmark Toolbar",
   "http://on.toolbar/bookmark"},
  {false, 1, {L"Folder"},
   L"New Bookmark",
   "http://domain/"},
  {false, 0, {},
   L"<Name>",
   "http://domain.com/q?a=%22er%22&b=%3C%20%20%3E"},
  {false, 0, {},
   L"Google Home Page",
   "http://www.google.com/"},
  {false, 0, {},
   L"\x4E2D\x6587",
   "http://chinese.site.cn/path?query=1#ref"},
  {false, 0, {},
   L"mail",
   "mailto:username@host"},
};

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

const PasswordInfo kFirefox2Passwords[] = {
  {"https://www.google.com/", "", "https://www.google.com/",
    L"", L"", L"", L"", true},
  {"http://localhost:8080/", "", "http://localhost:8080/corp.google.com",
    L"", L"http", L"", L"Http1+1abcdefg", false},
  {"http://localhost:8080/", "http://localhost:8080/", "http://localhost:8080/",
    L"loginuser", L"usr", L"loginpass", L"pwd", false},
  {"http://localhost:8080/", "http://localhost:8080/", "http://localhost:8080/",
    L"loginuser", L"firefox", L"loginpass", L"firefox", false},
  {"http://localhost/", "", "http://localhost/",
    L"loginuser", L"hello", L"", L"world", false},
};

struct KeywordInfo {
  const wchar_t* keyword;
  const char* url;
};

const KeywordInfo kFirefox2Keywords[] = {
  // Searh plugins
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
  { L"search.yahoo.com",
    "http://search.yahoo.com/search?p={searchTerms}&ei=UTF-8" },
  { L"flickr.com",
    "http://www.flickr.com/photos/tags/?q={searchTerms}" },
  { L"imdb.com",
    "http://www.imdb.com/find?q={searchTerms}" },
  { L"webster.com",
    "http://www.webster.com/cgi-bin/dictionary?va={searchTerms}" },
  // Search keywords.
  { L"google", "http://www.google.com/" },
  { L"< > & \" ' \\ /", "http://g.cn/"},
};

const int kDefaultFirefox2KeywordIndex = 8;

class FirefoxObserver : public ProfileWriter,
                        public importer::ImporterProgressObserver {
 public:
  FirefoxObserver() : ProfileWriter(NULL) {
    bookmark_count_ = 0;
    history_count_ = 0;
    password_count_ = 0;
    keyword_count_ = 0;
  }

  // importer::ImporterProgressObserver:
  virtual void ImportStarted() OVERRIDE {}
  virtual void ImportItemStarted(importer::ImportItem item) OVERRIDE {}
  virtual void ImportItemEnded(importer::ImportItem item) OVERRIDE {}
  virtual void ImportEnded() OVERRIDE {
    MessageLoop::current()->Quit();
    EXPECT_EQ(arraysize(kFirefox2Bookmarks), bookmark_count_);
    EXPECT_EQ(1U, history_count_);
    EXPECT_EQ(arraysize(kFirefox2Passwords), password_count_);
    EXPECT_EQ(arraysize(kFirefox2Keywords), keyword_count_);
  }

  virtual bool BookmarkModelIsLoaded() const OVERRIDE {
    // Profile is ready for writing.
    return true;
  }

  virtual bool TemplateURLServiceIsLoaded() const OVERRIDE {
    return true;
  }

  virtual void AddPasswordForm(const content::PasswordForm& form) OVERRIDE {
    PasswordInfo p = kFirefox2Passwords[password_count_];
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
    ASSERT_EQ(1U, page.size());
    EXPECT_EQ("http://en-us.www.mozilla.com/", page[0].url().spec());
    EXPECT_EQ(ASCIIToUTF16("Firefox Updated"), page[0].title());
    EXPECT_EQ(history::SOURCE_FIREFOX_IMPORTED, visit_source);
    ++history_count_;
  }

  virtual void AddBookmarks(const std::vector<BookmarkEntry>& bookmarks,
                            const string16& top_level_folder_name) OVERRIDE {
    for (size_t i = 0; i < bookmarks.size(); ++i) {
      if (FindBookmarkEntry(bookmarks[i], kFirefox2Bookmarks,
                            arraysize(kFirefox2Bookmarks)))
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
      for (size_t j = 0; j < arraysize(kFirefox2Keywords); ++j) {
        if (template_urls[i]->keyword() ==
            WideToUTF16Hack(kFirefox2Keywords[j].keyword)) {
          EXPECT_EQ(kFirefox2Keywords[j].url, template_urls[i]->url());
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found);
      ++keyword_count_;
    }
  }

  virtual void AddFavicons(
      const std::vector<history::ImportedFaviconUsage>& favicons) OVERRIDE {
  }

 private:
  virtual ~FirefoxObserver() {}

  size_t bookmark_count_;
  size_t history_count_;
  size_t password_count_;
  size_t keyword_count_;
};

const BookmarkInfo kFirefox3Bookmarks[] = {
  {true, 1, {L"Bookmarks Toolbar"},
    L"Toolbar",
    "http://site/"},
  {false, 0, {},
    L"Title",
    "http://www.google.com/"},
};

const PasswordInfo kFirefox3Passwords[] = {
  {"http://localhost:8080/", "http://localhost:8080/", "http://localhost:8080/",
    L"loginuser", L"abc", L"loginpass", L"123", false},
  {"http://localhost:8080/", "", "http://localhost:8080/localhost",
    L"", L"http", L"", L"Http1+1abcdefg", false},
};

const KeywordInfo kFirefox3Keywords[] = {
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

const int kDefaultFirefox3KeywordIndex = 8;

class Firefox3Observer : public ProfileWriter,
                         public importer::ImporterProgressObserver {
 public:
  Firefox3Observer()
      : ProfileWriter(NULL), bookmark_count_(0), history_count_(0),
        password_count_(0), keyword_count_(0), import_search_engines_(true) {
  }

  explicit Firefox3Observer(bool import_search_engines)
      : ProfileWriter(NULL), bookmark_count_(0), history_count_(0),
        password_count_(0), keyword_count_(0),
        import_search_engines_(import_search_engines) {
  }

  // importer::ImporterProgressObserver:
  virtual void ImportStarted() OVERRIDE {}
  virtual void ImportItemStarted(importer::ImportItem item) OVERRIDE {}
  virtual void ImportItemEnded(importer::ImportItem item) OVERRIDE {}
  virtual void ImportEnded() OVERRIDE {
    MessageLoop::current()->Quit();
    EXPECT_EQ(arraysize(kFirefox3Bookmarks), bookmark_count_);
    EXPECT_EQ(1U, history_count_);
    EXPECT_EQ(arraysize(kFirefox3Passwords), password_count_);
    if (import_search_engines_)
      EXPECT_EQ(arraysize(kFirefox3Keywords), keyword_count_);
  }

  virtual bool BookmarkModelIsLoaded() const OVERRIDE {
    // Profile is ready for writing.
    return true;
  }

  virtual bool TemplateURLServiceIsLoaded() const OVERRIDE {
    return true;
  }

  virtual void AddPasswordForm(const content::PasswordForm& form) OVERRIDE {
    PasswordInfo p = kFirefox3Passwords[password_count_];
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

  virtual void AddBookmarks(const std::vector<BookmarkEntry>& bookmarks,
                            const string16& top_level_folder_name) OVERRIDE {
    for (size_t i = 0; i < bookmarks.size(); ++i) {
      if (FindBookmarkEntry(bookmarks[i], kFirefox3Bookmarks,
                            arraysize(kFirefox3Bookmarks)))
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
      for (size_t j = 0; j < arraysize(kFirefox3Keywords); ++j) {
        if (template_urls[i]->keyword() ==
            WideToUTF16Hack(kFirefox3Keywords[j].keyword)) {
          EXPECT_EQ(kFirefox3Keywords[j].url, template_urls[i]->url());
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found);
      ++keyword_count_;
    }
  }

  virtual void AddFavicons(
      const std::vector<history::ImportedFaviconUsage>& favicons) OVERRIDE {
  }

 private:
  virtual ~Firefox3Observer() {}

  size_t bookmark_count_;
  size_t history_count_;
  size_t password_count_;
  size_t keyword_count_;
  bool import_search_engines_;
};

}  // namespace

class FirefoxProfileImporterTest : public ImporterTest {
 protected:
  virtual void SetUp() OVERRIDE {
    ImporterTest::SetUp();
    // Creates a new profile in a new subdirectory in the temp directory.
    base::FilePath test_path = temp_dir_.path().AppendASCII("ImporterTest");
    file_util::Delete(test_path, true);
    file_util::CreateDirectory(test_path);
    profile_path_ = test_path.AppendASCII("profile");
    app_path_ = test_path.AppendASCII("app");
    file_util::CreateDirectory(app_path_);
  }

  void Firefox3xImporterTest(std::string profile_dir,
                             importer::ImporterProgressObserver* observer,
                             ProfileWriter* writer,
                             bool import_search_plugins) {
    base::FilePath data_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
    data_path = data_path.AppendASCII(profile_dir);
    ASSERT_TRUE(file_util::CopyDirectory(data_path, profile_path_, true));
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
    data_path = data_path.AppendASCII("firefox3_nss");
    ASSERT_TRUE(file_util::CopyDirectory(data_path, profile_path_, false));

    base::FilePath search_engine_path = app_path_;
    search_engine_path = search_engine_path.AppendASCII("searchplugins");
    file_util::CreateDirectory(search_engine_path);
    if (import_search_plugins) {
      ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
      data_path = data_path.AppendASCII("firefox3_searchplugins");
      if (!file_util::PathExists(data_path)) {
        // TODO(maruel):  Create search test data that we can open source!
        LOG(ERROR) << L"Missing internal test data";
        return;
      }
      ASSERT_TRUE(file_util::CopyDirectory(data_path,
                                           search_engine_path, false));
    }

    MessageLoop* loop = MessageLoop::current();
    importer::SourceProfile source_profile;
    source_profile.importer_type = importer::TYPE_FIREFOX3;
    source_profile.app_path = app_path_;
    source_profile.source_path = profile_path_;
    scoped_refptr<ImporterHost> host(new ImporterHost);
    host->SetObserver(observer);
    int items = importer::HISTORY | importer::PASSWORDS | importer::FAVORITES;
    if (import_search_plugins)
      items = items | importer::SEARCH_ENGINES;
    loop->PostTask(FROM_HERE, base::Bind(
        &ImporterHost::StartImportSettings, host.get(), source_profile,
        profile_.get(), items, make_scoped_refptr(writer), true));
    loop->Run();
  }

  base::FilePath profile_path_;
  base::FilePath app_path_;
};

TEST_F(FirefoxProfileImporterTest, MAYBE_IMPORTER(Firefox2Importer)) {
  base::FilePath data_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
  data_path = data_path.AppendASCII("firefox2_profile");
  ASSERT_TRUE(file_util::CopyDirectory(data_path, profile_path_, true));
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
  data_path = data_path.AppendASCII("firefox2_nss");
  ASSERT_TRUE(file_util::CopyDirectory(data_path, profile_path_, false));

  base::FilePath search_engine_path = app_path_;
  search_engine_path = search_engine_path.AppendASCII("searchplugins");
  file_util::CreateDirectory(search_engine_path);
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
  data_path = data_path.AppendASCII("firefox2_searchplugins");
  if (!file_util::PathExists(data_path)) {
    // TODO(maruel):  Create test data that we can open source!
    LOG(ERROR) << L"Missing internal test data";
    return;
  }
  ASSERT_TRUE(file_util::CopyDirectory(data_path, search_engine_path, false));

  MessageLoop* loop = MessageLoop::current();
  scoped_refptr<ImporterHost> host(new ImporterHost);
  FirefoxObserver* observer = new FirefoxObserver();
  host->SetObserver(observer);
  importer::SourceProfile source_profile;
  source_profile.importer_type = importer::TYPE_FIREFOX2;
  source_profile.app_path = app_path_;
  source_profile.source_path = profile_path_;

  loop->PostTask(FROM_HERE, base::Bind(
      &ImporterHost::StartImportSettings,
      host.get(),
      source_profile,
      profile_.get(),
      importer::HISTORY | importer::PASSWORDS |
      importer::FAVORITES | importer::SEARCH_ENGINES,
      make_scoped_refptr(observer),
      true));
  loop->Run();
}

TEST_F(FirefoxProfileImporterTest, MAYBE_IMPORTER(Firefox30Importer)) {
  scoped_refptr<Firefox3Observer> observer(new Firefox3Observer());
  Firefox3xImporterTest("firefox3_profile", observer.get(), observer.get(),
                        true);
}

TEST_F(FirefoxProfileImporterTest, MAYBE_IMPORTER(Firefox35Importer)) {
  bool import_search_engines = false;
  scoped_refptr<Firefox3Observer> observer(
      new Firefox3Observer(import_search_engines));
  Firefox3xImporterTest("firefox35_profile", observer.get(), observer.get(),
                        import_search_engines);
}

// The following 2 tests require the use of the NSSDecryptor, on OSX this needs
// to run in a separate process, so we use a proxy object so we can share the
// same test between platforms.
TEST(FirefoxImporterTest, MAYBE_NSS(Firefox2NSS3Decryptor)) {
  base::FilePath nss_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &nss_path));
#ifdef OS_MACOSX
  nss_path = nss_path.AppendASCII("firefox2_nss_mac");
#else
  nss_path = nss_path.AppendASCII("firefox2_nss");
#endif  // !OS_MACOSX
  base::FilePath db_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &db_path));
  db_path = db_path.AppendASCII("firefox2_profile");

  FFUnitTestDecryptorProxy decryptor_proxy;
  ASSERT_TRUE(decryptor_proxy.Setup(nss_path));

  ASSERT_TRUE(decryptor_proxy.DecryptorInit(nss_path, db_path));
  EXPECT_EQ(ASCIIToUTF16("hello"),
      decryptor_proxy.Decrypt("MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECBJ"
                              "M63MpT9rtBAjMCm7qo/EhlA=="));
  // Test UTF-16 encoding.
  EXPECT_EQ(WideToUTF16(L"\x4E2D"),
      decryptor_proxy.Decrypt("MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECN9"
                              "OQ5ZFmhb8BAiFo1Z+fUvaIQ=="));
}

TEST(FirefoxImporterTest, MAYBE_NSS(Firefox3NSS3Decryptor)) {
  base::FilePath nss_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &nss_path));
#ifdef OS_MACOSX
  nss_path = nss_path.AppendASCII("firefox3_nss_mac");
#else
  nss_path = nss_path.AppendASCII("firefox3_nss");
#endif  // !OS_MACOSX
  base::FilePath db_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &db_path));
  db_path = db_path.AppendASCII("firefox3_profile");

  FFUnitTestDecryptorProxy decryptor_proxy;
  ASSERT_TRUE(decryptor_proxy.Setup(nss_path));

  ASSERT_TRUE(decryptor_proxy.DecryptorInit(nss_path, db_path));
  EXPECT_EQ(ASCIIToUTF16("hello"),
      decryptor_proxy.Decrypt("MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECKa"
                              "jtRg4qFSHBAhv9luFkXgDJA=="));
  // Test UTF-16 encoding.
  EXPECT_EQ(WideToUTF16(L"\x4E2D"),
      decryptor_proxy.Decrypt("MDIEEPgAAAAAAAAAAAAAAAAAAAEwFAYIKoZIhvcNAwcECLW"
                              "qqiccfQHWBAie74hxnULxlw=="));
}

TEST(FirefoxImporterTest, Firefox2BookmarkParse) {
  bool result;

  // Tests charset.
  std::string charset;
  result = Firefox2Importer::ParseCharsetFromLine(
      "<META HTTP-EQUIV=\"Content-Type\" "
      "CONTENT=\"text/html; charset=UTF-8\">",
      &charset);
  EXPECT_TRUE(result);
  EXPECT_EQ("UTF-8", charset);

  // Escaped characters in name.
  string16 folder_name;
  bool is_toolbar_folder;
  base::Time folder_add_date;
  result = Firefox2Importer::ParseFolderNameFromLine(
      "<DT><H3 ADD_DATE=\"1207558707\" >&lt; &gt;"
      " &amp; &quot; &#39; \\ /</H3>",
      charset, &folder_name, &is_toolbar_folder, &folder_add_date);
  EXPECT_TRUE(result);
  EXPECT_EQ(ASCIIToUTF16("< > & \" ' \\ /"), folder_name);
  EXPECT_FALSE(is_toolbar_folder);
  EXPECT_TRUE(base::Time::FromTimeT(1207558707) == folder_add_date);

  // Empty name and toolbar folder attribute.
  result = Firefox2Importer::ParseFolderNameFromLine(
      "<DT><H3 PERSONAL_TOOLBAR_FOLDER=\"true\"></H3>",
      charset, &folder_name, &is_toolbar_folder, &folder_add_date);
  EXPECT_TRUE(result);
  EXPECT_EQ(string16(), folder_name);
  EXPECT_TRUE(is_toolbar_folder);

  // Unicode characters in title and shortcut.
  string16 title;
  GURL url, favicon;
  string16 shortcut;
  string16 post_data;
  base::Time add_date;
  result = Firefox2Importer::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://chinese.site.cn/path?query=1#ref\" "
      "SHORTCUTURL=\"\xE4\xB8\xAD\">\xE4\xB8\xAD\xE6\x96\x87</A>",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_TRUE(result);
  EXPECT_EQ(L"\x4E2D\x6587", UTF16ToWide(title));
  EXPECT_EQ("http://chinese.site.cn/path?query=1#ref", url.spec());
  EXPECT_EQ(L"\x4E2D", UTF16ToWide(shortcut));
  EXPECT_EQ(string16(), post_data);
  EXPECT_TRUE(base::Time() == add_date);

  // No shortcut, and url contains %22 ('"' character).
  result = Firefox2Importer::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://domain.com/?q=%22<>%22\">name</A>",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_TRUE(result);
  EXPECT_EQ(ASCIIToUTF16("name"), title);
  EXPECT_EQ("http://domain.com/?q=%22%3C%3E%22", url.spec());
  EXPECT_EQ(string16(), shortcut);
  EXPECT_EQ(string16(), post_data);
  EXPECT_TRUE(base::Time() == add_date);

  result = Firefox2Importer::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://domain.com/?g=&quot;\"\">name</A>",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_TRUE(result);
  EXPECT_EQ(ASCIIToUTF16("name"), title);
  EXPECT_EQ("http://domain.com/?g=%22", url.spec());
  EXPECT_EQ(string16(), shortcut);
  EXPECT_EQ(string16(), post_data);
  EXPECT_TRUE(base::Time() == add_date);

  // Creation date.
  result = Firefox2Importer::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://site/\" ADD_DATE=\"1121301154\">name</A>",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_TRUE(result);
  EXPECT_EQ(ASCIIToUTF16("name"), title);
  EXPECT_EQ(GURL("http://site/"), url);
  EXPECT_EQ(string16(), shortcut);
  EXPECT_EQ(string16(), post_data);
  EXPECT_TRUE(base::Time::FromTimeT(1121301154) == add_date);

  // Post-data
  result = Firefox2Importer::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://localhost:8080/test/hello.html\" ADD_DATE=\""
      "1212447159\" LAST_VISIT=\"1212447251\" LAST_MODIFIED=\"1212447248\""
      "SHORTCUTURL=\"post\" ICON=\"data:\" POST_DATA=\"lname%3D%25s\""
      "LAST_CHARSET=\"UTF-8\" ID=\"rdf:#$weKaR3\">Test Post keyword</A>",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_TRUE(result);
  EXPECT_EQ(ASCIIToUTF16("Test Post keyword"), title);
  EXPECT_EQ("http://localhost:8080/test/hello.html", url.spec());
  EXPECT_EQ(ASCIIToUTF16("post"), shortcut);
  EXPECT_EQ(ASCIIToUTF16("lname%3D%25s"), post_data);
  EXPECT_TRUE(base::Time::FromTimeT(1212447159) == add_date);

  // Invalid case.
  result = Firefox2Importer::ParseBookmarkFromLine(
      "<DT><A HREF=\"http://domain.com/?q=%22",
      charset, &title, &url, &favicon, &shortcut, &add_date, &post_data);
  EXPECT_FALSE(result);
  EXPECT_EQ(string16(), title);
  EXPECT_EQ("", url.spec());
  EXPECT_EQ(string16(), shortcut);
  EXPECT_EQ(string16(), post_data);
  EXPECT_TRUE(base::Time() == add_date);

  // Epiphany format.
  result = Firefox2Importer::ParseMinimumBookmarkFromLine(
      "<dt><a href=\"http://www.google.com/\">Google</a></dt>",
      charset, &title, &url);
  EXPECT_TRUE(result);
  EXPECT_EQ(ASCIIToUTF16("Google"), title);
  EXPECT_EQ("http://www.google.com/", url.spec());
}

TEST(FirefoxImporterTest, Firefox2BookmarkFileImport) {
  base::FilePath path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("firefox2_importer");

  // Import all bookmarks from a file which include an empty folder entry.
  base::FilePath empty_folder_path = path.AppendASCII("empty_folder.html");
  std::set<GURL> default_urls;
  Firefox2Importer* importer = new Firefox2Importer();
  importer->AddRef();
  std::vector<ProfileWriter::BookmarkEntry> bookmarks;
  importer->ImportBookmarksFile(empty_folder_path, default_urls,
                                importer, &bookmarks, NULL, NULL);
  EXPECT_EQ(3U, bookmarks.size());
  std::vector<ProfileWriter::BookmarkEntry>::iterator it;
  ProfileWriter::BookmarkEntry entry;
  std::vector<string16>::iterator path_it;
  if (bookmarks.size() == 3) {
    it = bookmarks.begin();
    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("Empty"), entry.title);
    EXPECT_TRUE(entry.is_folder);
    EXPECT_EQ(base::Time::FromTimeT(1295938143), entry.creation_time);
    EXPECT_EQ(1U, entry.path.size());
    if (entry.path.size() == 1)
      EXPECT_EQ(ASCIIToUTF16("Empty's Parent"), entry.path.front());

    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("[Tamura Yukari.com]"), entry.title);
    EXPECT_FALSE(entry.is_folder);
    EXPECT_EQ(base::Time::FromTimeT(1234567890), entry.creation_time);
    EXPECT_EQ(1U, entry.path.size());
    if (entry.path.size() == 1)
      EXPECT_EQ(ASCIIToUTF16("Not Empty"), entry.path.front());
    EXPECT_EQ("http://www.tamurayukari.com/", entry.url.spec());

    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("Google"), entry.title);
    EXPECT_FALSE(entry.is_folder);
    EXPECT_EQ(base::Time::FromTimeT(0000000000), entry.creation_time);
    EXPECT_EQ(1U, entry.path.size());
    if (entry.path.size() == 1)
      EXPECT_EQ(ASCIIToUTF16("Not Empty But Default"), entry.path.front());
    EXPECT_EQ("http://www.google.com/", entry.url.spec());
  }

  // Import non-default bookmarks from a file.
  bookmarks.clear();
  default_urls.insert(GURL("http://www.google.com/"));
  importer->ImportBookmarksFile(empty_folder_path, default_urls,
                                importer, &bookmarks, NULL, NULL);
  EXPECT_EQ(2U, bookmarks.size());
  if (bookmarks.size() == 2) {
    it = bookmarks.begin();
    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("Empty"), entry.title);
    EXPECT_TRUE(entry.is_folder);
    EXPECT_EQ(base::Time::FromTimeT(1295938143), entry.creation_time);
    EXPECT_EQ(1U, entry.path.size());
    if (entry.path.size() == 1)
      EXPECT_EQ(ASCIIToUTF16("Empty's Parent"), entry.path.front());

    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("[Tamura Yukari.com]"), entry.title);
    EXPECT_FALSE(entry.is_folder);
    EXPECT_EQ(base::Time::FromTimeT(1234567890), entry.creation_time);
    EXPECT_EQ(1U, entry.path.size());
    if (entry.path.size() == 1)
      EXPECT_EQ(ASCIIToUTF16("Not Empty"), entry.path.front());
    EXPECT_EQ("http://www.tamurayukari.com/", entry.url.spec());
  }

  // Import Epiphany bookmarks from a file
  base::FilePath epiphany_path = path.AppendASCII("epiphany.html");
  bookmarks.clear();
  default_urls.clear();
  importer->ImportBookmarksFile(epiphany_path, default_urls,
                                importer, &bookmarks, NULL, NULL);
  EXPECT_EQ(2U, bookmarks.size());
  if (bookmarks.size() == 2) {
    it = bookmarks.begin();
    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("[Tamura Yukari.com]"), entry.title);
    EXPECT_EQ("http://www.tamurayukari.com/", entry.url.spec());
    EXPECT_EQ(0U, entry.path.size());
    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("Google"), entry.title);
    EXPECT_EQ("http://www.google.com/", entry.url.spec());
    EXPECT_EQ(0U, entry.path.size());
  }

  // Import Epiphany bookmarks from a file to bookmark bar.
  bookmarks.clear();
  default_urls.clear();
  importer->ImportBookmarksFile(epiphany_path, default_urls,
                                importer, &bookmarks, NULL, NULL);
  EXPECT_EQ(2U, bookmarks.size());
  if (bookmarks.size() == 2) {
    it = bookmarks.begin();
    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("[Tamura Yukari.com]"), entry.title);
    EXPECT_EQ("http://www.tamurayukari.com/", entry.url.spec());
    EXPECT_EQ(0U, entry.path.size());
    entry = *it++;
    EXPECT_EQ(ASCIIToUTF16("Google"), entry.title);
    EXPECT_EQ("http://www.google.com/", entry.url.spec());
    EXPECT_EQ(0U, entry.path.size());
  }

  importer->Release();
}
