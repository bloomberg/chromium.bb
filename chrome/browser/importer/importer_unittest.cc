// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <unknwn.h>
#include <intshcut.h>
#include <pstore.h>
#include <urlhist.h>
#include <shlguid.h>
#endif

#include <vector>

#include "app/win/scoped_com_initializer.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/chrome_paths.h"
#include "webkit/glue/password_form.h"

#if defined(OS_WIN)
#include "base/scoped_comptr_win.h"
#include "chrome/browser/importer/ie_importer.h"
#include "chrome/browser/password_manager/ie7_password.h"
#endif

using importer::FAVORITES;
using importer::FIREFOX2;
using importer::FIREFOX3;
using importer::HISTORY;
using importer::ImportItem;
#if defined(OS_WIN)
using importer::MS_IE;
#endif
using importer::PASSWORDS;
using importer::SEARCH_ENGINES;
using webkit_glue::PasswordForm;

// TODO(estade): some of these are disabled on mac. http://crbug.com/48007
#if defined(OS_MACOSX)
#define MAYBE(x) DISABLED_##x
#else
#define MAYBE(x) x
#endif

class ImporterTest : public testing::Test {
 public:
  ImporterTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE, &message_loop_) {}
 protected:
  virtual void SetUp() {
    // Creates a new profile in a new subdirectory in the temp directory.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_path_));
    test_path_ = test_path_.AppendASCII("ImporterTest");
    file_util::Delete(test_path_, true);
    file_util::CreateDirectory(test_path_);
    profile_path_ = test_path_.AppendASCII("profile");
    app_path_ = test_path_.AppendASCII("app");
    file_util::CreateDirectory(app_path_);
  }

  virtual void TearDown() {
    // Deletes the profile and cleans up the profile directory.
    ASSERT_TRUE(file_util::Delete(test_path_, true));
    ASSERT_FALSE(file_util::PathExists(test_path_));
  }

  void Firefox3xImporterTest(std::string profile_dir,
                             ImporterHost::Observer* observer,
                             ProfileWriter* writer,
                             bool import_search_plugins) {
    FilePath data_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
    data_path = data_path.AppendASCII(profile_dir);
    ASSERT_TRUE(file_util::CopyDirectory(data_path, profile_path_, true));
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
    data_path = data_path.AppendASCII("firefox3_nss");
    ASSERT_TRUE(file_util::CopyDirectory(data_path, profile_path_, false));

    FilePath search_engine_path = app_path_;
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
    ProfileInfo profile_info;
    profile_info.browser_type = FIREFOX3;
    profile_info.app_path = app_path_;
    profile_info.source_path = profile_path_;
    scoped_refptr<ImporterHost> host(new ImporterHost);
    host->SetObserver(observer);
    int items = HISTORY | PASSWORDS | FAVORITES;
    if (import_search_plugins)
      items = items | SEARCH_ENGINES;
    loop->PostTask(FROM_HERE, NewRunnableMethod(host.get(),
        &ImporterHost::StartImportSettings, profile_info,
        static_cast<Profile*>(NULL), items, make_scoped_refptr(writer), true));
    loop->Run();
  }

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
  FilePath test_path_;
  FilePath profile_path_;
  FilePath app_path_;
};

const int kMaxPathSize = 5;

typedef struct {
  const bool in_toolbar;
  const size_t path_size;
  const wchar_t* path[kMaxPathSize];
  const wchar_t* title;
  const char* url;
} BookmarkList;

typedef struct {
  const char* origin;
  const char* action;
  const char* realm;
  const wchar_t* username_element;
  const wchar_t* username;
  const wchar_t* password_element;
  const wchar_t* password;
  bool blacklisted;
} PasswordList;

// Returns true if the |entry| is in the |list|.
bool FindBookmarkEntry(const ProfileWriter::BookmarkEntry& entry,
                       const BookmarkList* list, int list_size) {
  for (int i = 0; i < list_size; ++i) {
    if (list[i].in_toolbar == entry.in_toolbar &&
        list[i].path_size == entry.path.size() &&
        list[i].url == entry.url.spec() &&
        list[i].title == entry.title) {
      bool equal = true;
      for (size_t k = 0; k < list[i].path_size; ++k)
        if (list[i].path[k] != entry.path[k]) {
          equal = false;
          break;
        }

      if (equal)
        return true;
    }
  }
  return false;
}

#if defined(OS_WIN)
static const BookmarkList kIEBookmarks[] = {
  {true, 0, {},
   L"TheLink",
   "http://www.links-thelink.com/"},
  {true, 1, {L"SubFolderOfLinks"},
   L"SubLink",
   "http://www.links-sublink.com/"},
  {false, 0, {},
   L"Google Home Page",
   "http://www.google.com/"},
  {false, 0, {},
   L"TheLink",
   "http://www.links-thelink.com/"},
  {false, 1, {L"SubFolder"},
   L"Title",
   "http://www.link.com/"},
  {false, 0, {},
   L"WithPortAndQuery",
   "http://host:8080/cgi?q=query"},
  {false, 1, {L"a"},
   L"\x4E2D\x6587",
   "http://chinese-title-favorite/"},
};

static const wchar_t* kIEIdentifyUrl =
    L"http://A79029D6-753E-4e27-B807-3D46AB1545DF.com:8080/path?key=value";
static const wchar_t* kIEIdentifyTitle =
    L"Unittest GUID";

bool IsWindowsVista() {
  OSVERSIONINFO info = {0};
  info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx(&info);
  return (info.dwMajorVersion >=6);
}

class TestObserver : public ProfileWriter,
                     public ImporterHost::Observer {
 public:
  TestObserver() : ProfileWriter(NULL) {
    bookmark_count_ = 0;
    history_count_ = 0;
    password_count_ = 0;
  }

  virtual void ImportItemStarted(importer::ImportItem item) {}
  virtual void ImportItemEnded(importer::ImportItem item) {}
  virtual void ImportStarted() {}
  virtual void ImportEnded() {
    MessageLoop::current()->Quit();
    EXPECT_EQ(arraysize(kIEBookmarks), bookmark_count_);
    EXPECT_EQ(1, history_count_);
#if 0  // This part of the test is disabled. See bug #2466
    if (IsWindowsVista())
      EXPECT_EQ(0, password_count_);
    else
      EXPECT_EQ(1, password_count_);
#endif
  }

  virtual bool BookmarkModelIsLoaded() const {
    // Profile is ready for writing.
    return true;
  }

  virtual bool TemplateURLModelIsLoaded() const {
    return true;
  }

  virtual void AddPasswordForm(const PasswordForm& form) {
    // Importer should obtain this password form only.
    EXPECT_EQ(GURL("http://localhost:8080/security/index.htm"), form.origin);
    EXPECT_EQ("http://localhost:8080/", form.signon_realm);
    EXPECT_EQ(L"user", form.username_element);
    EXPECT_EQ(L"1", form.username_value);
    EXPECT_EQ(L"", form.password_element);
    EXPECT_EQ(L"2", form.password_value);
    EXPECT_EQ("", form.action.spec());
    ++password_count_;
  }

  virtual void AddHistoryPage(const std::vector<history::URLRow>& page,
                              history::VisitSource visit_source) {
    // Importer should read the specified URL.
    for (size_t i = 0; i < page.size(); ++i) {
      if (page[i].title() == kIEIdentifyTitle &&
          page[i].url() == GURL(kIEIdentifyUrl))
        ++history_count_;
    }
    EXPECT_EQ(history::SOURCE_IE_IMPORTED, visit_source);
  }

  virtual void AddBookmarkEntry(const std::vector<BookmarkEntry>& bookmark,
                                const std::wstring& first_folder_name,
                                int options) {
    // Importer should import the IE Favorites folder the same as the list.
    for (size_t i = 0; i < bookmark.size(); ++i) {
      if (FindBookmarkEntry(bookmark[i], kIEBookmarks,
                            arraysize(kIEBookmarks)))
        ++bookmark_count_;
    }
  }

  virtual void AddKeyword(std::vector<TemplateURL*> template_url,
                          int default_keyword_index) {
    // TODO(jcampan): bug 1169230: we should test keyword importing for IE.
    // In order to do that we'll probably need to mock the Windows registry.
    NOTREACHED();
    STLDeleteContainerPointers(template_url.begin(), template_url.end());
  }

 private:
  ~TestObserver() {}

  size_t bookmark_count_;
  size_t history_count_;
  size_t password_count_;
};

bool CreateUrlFile(std::wstring file, std::wstring url) {
  ScopedComPtr<IUniformResourceLocator> locator;
  HRESULT result = locator.CreateInstance(CLSID_InternetShortcut, NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return false;
  ScopedComPtr<IPersistFile> persist_file;
  result = persist_file.QueryFrom(locator);
  if (FAILED(result))
    return false;
  result = locator->SetURL(url.c_str(), 0);
  if (FAILED(result))
    return false;
  result = persist_file->Save(file.c_str(), TRUE);
  if (FAILED(result))
    return false;
  return true;
}

void ClearPStoreType(IPStore* pstore, const GUID* type, const GUID* subtype) {
  ScopedComPtr<IEnumPStoreItems, NULL> item;
  HRESULT result = pstore->EnumItems(0, type, subtype, 0, item.Receive());
  if (result == PST_E_OK) {
    wchar_t* item_name;
    while (SUCCEEDED(item->Next(1, &item_name, 0))) {
      pstore->DeleteItem(0, type, subtype, item_name, NULL, 0);
      CoTaskMemFree(item_name);
    }
  }
  pstore->DeleteSubtype(0, type, subtype, 0);
  pstore->DeleteType(0, type, 0);
}

void WritePStore(IPStore* pstore, const GUID* type, const GUID* subtype) {
  struct PStoreItem {
    wchar_t* name;
    int data_size;
    char* data;
  } items[] = {
    {L"http://localhost:8080/security/index.htm#ref:StringData", 8,
     "\x31\x00\x00\x00\x32\x00\x00\x00"},
    {L"http://localhost:8080/security/index.htm#ref:StringIndex", 20,
     "\x57\x49\x43\x4b\x18\x00\x00\x00\x02\x00"
     "\x00\x00\x2f\x00\x74\x00\x01\x00\x00\x00"},
    {L"user:StringData", 4,
     "\x31\x00\x00\x00"},
    {L"user:StringIndex", 20,
     "\x57\x49\x43\x4b\x18\x00\x00\x00\x01\x00"
     "\x00\x00\x2f\x00\x74\x00\x00\x00\x00\x00"},
  };

  for (int i = 0; i < arraysize(items); ++i) {
    HRESULT res = pstore->WriteItem(0, type, subtype, items[i].name,
        items[i].data_size, reinterpret_cast<BYTE*>(items[i].data),
        NULL, 0, 0);
    ASSERT_TRUE(res == PST_E_OK);
  }
}

TEST_F(ImporterTest, IEImporter) {
  // Sets up a favorites folder.
  app::win::ScopedCOMInitializer com_init;
  std::wstring path = test_path_.AppendASCII("Favorites").value();
  CreateDirectory(path.c_str(), NULL);
  CreateDirectory((path + L"\\SubFolder").c_str(), NULL);
  CreateDirectory((path + L"\\Links").c_str(), NULL);
  CreateDirectory((path + L"\\Links\\SubFolderOfLinks").c_str(), NULL);
  CreateDirectory((path + L"\\\x0061").c_str(), NULL);
  ASSERT_TRUE(CreateUrlFile(path + L"\\Google Home Page.url",
                            L"http://www.google.com/"));
  ASSERT_TRUE(CreateUrlFile(path + L"\\SubFolder\\Title.url",
                            L"http://www.link.com/"));
  ASSERT_TRUE(CreateUrlFile(path + L"\\TheLink.url",
                            L"http://www.links-thelink.com/"));
  ASSERT_TRUE(CreateUrlFile(path + L"\\WithPortAndQuery.url",
                            L"http://host:8080/cgi?q=query"));
  ASSERT_TRUE(CreateUrlFile(path + L"\\\x0061\\\x4E2D\x6587.url",
                            L"http://chinese-title-favorite/"));
  ASSERT_TRUE(CreateUrlFile(path + L"\\Links\\TheLink.url",
                            L"http://www.links-thelink.com/"));
  ASSERT_TRUE(CreateUrlFile(path + L"\\Links\\SubFolderOfLinks\\SubLink.url",
                            L"http://www.links-sublink.com/"));
  file_util::WriteFile(path + L"\\InvalidUrlFile.url", "x", 1);
  file_util::WriteFile(path + L"\\PlainTextFile.txt", "x", 1);

  // Sets up dummy password data.
  HRESULT res;
#if 0  // This part of the test is disabled. See bug #2466
  ScopedComPtr<IPStore> pstore;
  HMODULE pstorec_dll;
  GUID type = IEImporter::kUnittestGUID;
  GUID subtype = IEImporter::kUnittestGUID;
  // PStore is read-only in Windows Vista.
  if (!IsWindowsVista()) {
    typedef HRESULT (WINAPI *PStoreCreateFunc)(IPStore**, DWORD, DWORD, DWORD);
    pstorec_dll = LoadLibrary(L"pstorec.dll");
    PStoreCreateFunc PStoreCreateInstance =
        (PStoreCreateFunc)GetProcAddress(pstorec_dll, "PStoreCreateInstance");
    res = PStoreCreateInstance(pstore.Receive(), 0, 0, 0);
    ASSERT_TRUE(res == S_OK);
    ClearPStoreType(pstore, &type, &subtype);
    PST_TYPEINFO type_info;
    type_info.szDisplayName = L"TestType";
    type_info.cbSize = 8;
    pstore->CreateType(0, &type, &type_info, 0);
    pstore->CreateSubtype(0, &type, &subtype, &type_info, NULL, 0);
    WritePStore(pstore, &type, &subtype);
  }
#endif

  // Sets up a special history link.
  ScopedComPtr<IUrlHistoryStg2> url_history_stg2;
  res = url_history_stg2.CreateInstance(CLSID_CUrlHistory, NULL,
                                        CLSCTX_INPROC_SERVER);
  ASSERT_TRUE(res == S_OK);
  res = url_history_stg2->AddUrl(kIEIdentifyUrl, kIEIdentifyTitle, 0);
  ASSERT_TRUE(res == S_OK);

  // Starts to import the above settings.
  MessageLoop* loop = MessageLoop::current();
  scoped_refptr<ImporterHost> host(new ImporterHost);

  TestObserver* observer = new TestObserver();
  host->SetObserver(observer);
  ProfileInfo profile_info;
  profile_info.browser_type = MS_IE;
  profile_info.source_path = test_path_;

  loop->PostTask(FROM_HERE, NewRunnableMethod(host.get(),
      &ImporterHost::StartImportSettings, profile_info,
      static_cast<Profile*>(NULL), HISTORY | PASSWORDS | FAVORITES, observer,
      true));
  loop->Run();

  // Cleans up.
  url_history_stg2->DeleteUrl(kIEIdentifyUrl, 0);
  url_history_stg2.Release();
#if 0  // This part of the test is disabled. See bug #2466
  if (!IsWindowsVista()) {
    ClearPStoreType(pstore, &type, &subtype);
    // Releases it befor unload the dll.
    pstore.Release();
    FreeLibrary(pstorec_dll);
  }
#endif
}

TEST_F(ImporterTest, IE7Importer) {
  // This is the unencrypted values of my keys under Storage2.
  // The passwords have been manually changed to abcdef... but the size remains
  // the same.
  unsigned char data1[] = "\x0c\x00\x00\x00\x38\x00\x00\x00\x2c\x00\x00\x00"
                          "\x57\x49\x43\x4b\x18\x00\x00\x00\x02\x00\x00\x00"
                          "\x67\x00\x72\x00\x01\x00\x00\x00\x00\x00\x00\x00"
                          "\x00\x00\x00\x00\x4e\xfa\x67\x76\x22\x94\xc8\x01"
                          "\x08\x00\x00\x00\x12\x00\x00\x00\x4e\xfa\x67\x76"
                          "\x22\x94\xc8\x01\x0c\x00\x00\x00\x61\x00\x62\x00"
                          "\x63\x00\x64\x00\x65\x00\x66\x00\x67\x00\x68\x00"
                          "\x00\x00\x61\x00\x62\x00\x63\x00\x64\x00\x65\x00"
                          "\x66\x00\x67\x00\x68\x00\x69\x00\x6a\x00\x6b\x00"
                          "\x6c\x00\x00\x00";

  unsigned char data2[] = "\x0c\x00\x00\x00\x38\x00\x00\x00\x24\x00\x00\x00"
                          "\x57\x49\x43\x4b\x18\x00\x00\x00\x02\x00\x00\x00"
                          "\x67\x00\x72\x00\x01\x00\x00\x00\x00\x00\x00\x00"
                          "\x00\x00\x00\x00\xa8\xea\xf4\xe5\x9f\x9a\xc8\x01"
                          "\x09\x00\x00\x00\x14\x00\x00\x00\xa8\xea\xf4\xe5"
                          "\x9f\x9a\xc8\x01\x07\x00\x00\x00\x61\x00\x62\x00"
                          "\x63\x00\x64\x00\x65\x00\x66\x00\x67\x00\x68\x00"
                          "\x69\x00\x00\x00\x61\x00\x62\x00\x63\x00\x64\x00"
                          "\x65\x00\x66\x00\x67\x00\x00\x00";



  std::vector<unsigned char> decrypted_data1;
  decrypted_data1.resize(arraysize(data1));
  memcpy(&decrypted_data1.front(), data1, sizeof(data1));

  std::vector<unsigned char> decrypted_data2;
  decrypted_data2.resize(arraysize(data2));
  memcpy(&decrypted_data2.front(), data2, sizeof(data2));

  std::wstring password;
  std::wstring username;
  ASSERT_TRUE(ie7_password::GetUserPassFromData(decrypted_data1, &username,
                                                &password));
  EXPECT_EQ(L"abcdefgh", username);
  EXPECT_EQ(L"abcdefghijkl", password);

  ASSERT_TRUE(ie7_password::GetUserPassFromData(decrypted_data2, &username,
                                                &password));
  EXPECT_EQ(L"abcdefghi", username);
  EXPECT_EQ(L"abcdefg", password);
}
#endif  // defined(OS_WIN)

static const BookmarkList kFirefox2Bookmarks[] = {
  {true, 1, {L"Folder"},
   L"On Toolbar's Subfolder",
   "http://on.toolbar/bookmark/folder"},
  {true, 0, {},
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

static const PasswordList kFirefox2Passwords[] = {
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

typedef struct {
  const wchar_t* keyword;
  const char* url;
} KeywordList;

static const KeywordList kFirefox2Keywords[] = {
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

static const int kDefaultFirefox2KeywordIndex = 8;

class FirefoxObserver : public ProfileWriter,
                        public ImporterHost::Observer {
 public:
  FirefoxObserver() : ProfileWriter(NULL) {
    bookmark_count_ = 0;
    history_count_ = 0;
    password_count_ = 0;
    keyword_count_ = 0;
  }

  virtual void ImportItemStarted(importer::ImportItem item) {}
  virtual void ImportItemEnded(importer::ImportItem item) {}
  virtual void ImportStarted() {}
  virtual void ImportEnded() {
    MessageLoop::current()->Quit();
    EXPECT_EQ(arraysize(kFirefox2Bookmarks), bookmark_count_);
    EXPECT_EQ(1U, history_count_);
    EXPECT_EQ(arraysize(kFirefox2Passwords), password_count_);
    EXPECT_EQ(arraysize(kFirefox2Keywords), keyword_count_);
    EXPECT_EQ(kFirefox2Keywords[kDefaultFirefox2KeywordIndex].keyword,
              default_keyword_);
    EXPECT_EQ(kFirefox2Keywords[kDefaultFirefox2KeywordIndex].url,
              default_keyword_url_);
  }

  virtual bool BookmarkModelIsLoaded() const {
    // Profile is ready for writing.
    return true;
  }

  virtual bool TemplateURLModelIsLoaded() const {
    return true;
  }

  virtual void AddPasswordForm(const PasswordForm& form) {
    PasswordList p = kFirefox2Passwords[password_count_];
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

  virtual void AddHistoryPage(const std::vector<history::URLRow>& page,
                              history::VisitSource visit_source) {
    ASSERT_EQ(1U, page.size());
    EXPECT_EQ("http://en-us.www.mozilla.com/", page[0].url().spec());
    EXPECT_EQ(ASCIIToUTF16("Firefox Updated"), page[0].title());
    EXPECT_EQ(history::SOURCE_FIREFOX_IMPORTED, visit_source);
    ++history_count_;
  }

  virtual void AddBookmarkEntry(const std::vector<BookmarkEntry>& bookmark,
                                const std::wstring& first_folder_name,
                                int options) {
    for (size_t i = 0; i < bookmark.size(); ++i) {
      if (FindBookmarkEntry(bookmark[i], kFirefox2Bookmarks,
                            arraysize(kFirefox2Bookmarks)))
        ++bookmark_count_;
    }
  }

  virtual void AddKeywords(const std::vector<TemplateURL*>& template_urls,
                           int default_keyword_index,
                           bool unique_on_host_and_path) {
    for (size_t i = 0; i < template_urls.size(); ++i) {
      // The order might not be deterministic, look in the expected list for
      // that template URL.
      bool found = false;
      string16 keyword = template_urls[i]->keyword();
      for (size_t j = 0; j < arraysize(kFirefox2Keywords); ++j) {
        if (template_urls[i]->keyword() ==
            WideToUTF16Hack(kFirefox2Keywords[j].keyword)) {
          EXPECT_EQ(kFirefox2Keywords[j].url, template_urls[i]->url()->url());
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found);
      ++keyword_count_;
    }

    if (default_keyword_index != -1) {
      EXPECT_LT(default_keyword_index, static_cast<int>(template_urls.size()));
      TemplateURL* default_turl = template_urls[default_keyword_index];
      default_keyword_ = UTF16ToWideHack(default_turl->keyword());
      default_keyword_url_ = default_turl->url()->url();
    }

    STLDeleteContainerPointers(template_urls.begin(), template_urls.end());
  }

  void AddFavicons(const std::vector<history::ImportedFavIconUsage>& favicons) {
  }

 private:
  ~FirefoxObserver() {}

  size_t bookmark_count_;
  size_t history_count_;
  size_t password_count_;
  size_t keyword_count_;
  std::wstring default_keyword_;
  std::string default_keyword_url_;
};

TEST_F(ImporterTest, MAYBE(Firefox2Importer)) {
  FilePath data_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
  data_path = data_path.AppendASCII("firefox2_profile");
  ASSERT_TRUE(file_util::CopyDirectory(data_path, profile_path_, true));
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_path));
  data_path = data_path.AppendASCII("firefox2_nss");
  ASSERT_TRUE(file_util::CopyDirectory(data_path, profile_path_, false));

  FilePath search_engine_path = app_path_;
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
  ProfileInfo profile_info;
  profile_info.browser_type = FIREFOX2;
  profile_info.app_path = app_path_;
  profile_info.source_path = profile_path_;

  loop->PostTask(FROM_HERE, NewRunnableMethod(
      host.get(),
      &ImporterHost::StartImportSettings,
      profile_info,
      static_cast<Profile*>(NULL),
      HISTORY | PASSWORDS | FAVORITES | SEARCH_ENGINES,
      make_scoped_refptr(observer),
      true));
  loop->Run();
}

static const BookmarkList kFirefox3Bookmarks[] = {
  {true, 0, {},
    L"Toolbar",
    "http://site/"},
  {false, 0, {},
    L"Title",
    "http://www.google.com/"},
};

static const PasswordList kFirefox3Passwords[] = {
  {"http://localhost:8080/", "http://localhost:8080/", "http://localhost:8080/",
    L"loginuser", L"abc", L"loginpass", L"123", false},
  {"http://localhost:8080/", "", "http://localhost:8080/localhost",
    L"", L"http", L"", L"Http1+1abcdefg", false},
};

static const KeywordList kFirefox3Keywords[] = {
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

static const int kDefaultFirefox3KeywordIndex = 8;

class Firefox3Observer : public ProfileWriter,
                         public ImporterHost::Observer {
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

  virtual void ImportItemStarted(importer::ImportItem item) {}
  virtual void ImportItemEnded(importer::ImportItem item) {}
  virtual void ImportStarted() {}
  virtual void ImportEnded() {
    MessageLoop::current()->Quit();
    EXPECT_EQ(arraysize(kFirefox3Bookmarks), bookmark_count_);
    EXPECT_EQ(1U, history_count_);
    EXPECT_EQ(arraysize(kFirefox3Passwords), password_count_);
    if (import_search_engines_) {
      EXPECT_EQ(arraysize(kFirefox3Keywords), keyword_count_);
      EXPECT_EQ(kFirefox3Keywords[kDefaultFirefox3KeywordIndex].keyword,
                default_keyword_);
      EXPECT_EQ(kFirefox3Keywords[kDefaultFirefox3KeywordIndex].url,
                default_keyword_url_);
    }
  }

  virtual bool BookmarkModelIsLoaded() const {
    // Profile is ready for writing.
    return true;
  }

  virtual bool TemplateURLModelIsLoaded() const {
    return true;
  }

  virtual void AddPasswordForm(const PasswordForm& form) {
    PasswordList p = kFirefox3Passwords[password_count_];
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

  virtual void AddHistoryPage(const std::vector<history::URLRow>& page,
                              history::VisitSource visit_source) {
    ASSERT_EQ(3U, page.size());
    EXPECT_EQ("http://www.google.com/", page[0].url().spec());
    EXPECT_EQ(ASCIIToUTF16("Google"), page[0].title());
    EXPECT_EQ("http://www.google.com/", page[1].url().spec());
    EXPECT_EQ(ASCIIToUTF16("Google"), page[1].title());
    EXPECT_EQ("http://www.cs.unc.edu/~jbs/resources/perl/perl-cgi/programs/form1-POST.html",
              page[2].url().spec());
    EXPECT_EQ(ASCIIToUTF16("example form (POST)"), page[2].title());
    EXPECT_EQ(history::SOURCE_FIREFOX_IMPORTED, visit_source);
    ++history_count_;
  }

  virtual void AddBookmarkEntry(const std::vector<BookmarkEntry>& bookmark,
                                const std::wstring& first_folder_name,
                                int options) {
    for (size_t i = 0; i < bookmark.size(); ++i) {
      if (FindBookmarkEntry(bookmark[i], kFirefox3Bookmarks,
                            arraysize(kFirefox3Bookmarks)))
        ++bookmark_count_;
    }
  }

  void AddKeywords(const std::vector<TemplateURL*>& template_urls,
                  int default_keyword_index,
                  bool unique_on_host_and_path) {
    for (size_t i = 0; i < template_urls.size(); ++i) {
      // The order might not be deterministic, look in the expected list for
      // that template URL.
      bool found = false;
      string16 keyword = template_urls[i]->keyword();
      for (size_t j = 0; j < arraysize(kFirefox3Keywords); ++j) {
        if (template_urls[i]->keyword() ==
            WideToUTF16Hack(kFirefox3Keywords[j].keyword)) {
          EXPECT_EQ(kFirefox3Keywords[j].url, template_urls[i]->url()->url());
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found);
      ++keyword_count_;
    }

    if (default_keyword_index != -1) {
      EXPECT_LT(default_keyword_index, static_cast<int>(template_urls.size()));
      TemplateURL* default_turl = template_urls[default_keyword_index];
      default_keyword_ = UTF16ToWideHack(default_turl->keyword());
      default_keyword_url_ = default_turl->url()->url();
    }

    STLDeleteContainerPointers(template_urls.begin(), template_urls.end());
  }

  void AddFavicons(const std::vector<history::ImportedFavIconUsage>& favicons) {
  }

 private:
  ~Firefox3Observer() {}

  size_t bookmark_count_;
  size_t history_count_;
  size_t password_count_;
  size_t keyword_count_;
  bool import_search_engines_;
  std::wstring default_keyword_;
  std::string default_keyword_url_;
};

TEST_F(ImporterTest, MAYBE(Firefox30Importer)) {
  scoped_refptr<Firefox3Observer> observer(new Firefox3Observer());
  Firefox3xImporterTest("firefox3_profile", observer.get(), observer.get(),
                        true);
}

TEST_F(ImporterTest, MAYBE(Firefox35Importer)) {
  bool import_search_engines = false;
  scoped_refptr<Firefox3Observer> observer(
      new Firefox3Observer(import_search_engines));
  Firefox3xImporterTest("firefox35_profile", observer.get(), observer.get(),
                        import_search_engines);
}
