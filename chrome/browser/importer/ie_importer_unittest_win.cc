// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

// The order of these includes is important.
#include <windows.h>
#include <unknwn.h>
#include <intshcut.h>
#include <shlguid.h>
#include <urlhist.h>
#include <shlobj.h>
#include <propvarutil.h>

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/windows_version.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/ie_importer.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/importer/importer_unittest_utils.h"
#include "chrome/browser/importer/pstore_declarations.h"
#include "chrome/browser/password_manager/ie7_password.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/common/password_form.h"

namespace {

const char16 kUnitTestRegistrySubKey[] = L"SOFTWARE\\Chromium Unit Tests";
const char16 kUnitTestUserOverrideSubKey[] =
    L"SOFTWARE\\Chromium Unit Tests\\HKCU Override";

const BookmarkInfo kIEBookmarks[] = {
  {true, 2, {L"Links", L"SubFolderOfLinks"},
   L"SubLink",
   "http://www.links-sublink.com/"},
  {true, 1, {L"Links"},
   L"TheLink",
   "http://www.links-thelink.com/"},
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
  {false, 0, {},
   L"SubFolder",
   "http://www.subfolder.com/"},
};

const BookmarkInfo kIESortedBookmarks[] = {
  {false, 0, {}, L"a", "http://www.google.com/0"},
  {false, 1, {L"b"}, L"a", "http://www.google.com/1"},
  {false, 1, {L"b"}, L"b", "http://www.google.com/2"},
  {false, 0, {}, L"c", "http://www.google.com/3"},
};

const char16 kIEIdentifyUrl[] =
    L"http://A79029D6-753E-4e27-B807-3D46AB1545DF.com:8080/path?key=value";
const char16 kIEIdentifyTitle[] =
    L"Unittest GUID";

const char16 kIEFavoritesOrderKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\"
    L"MenuOrder\\Favorites";

const char16 kFaviconStreamSuffix[] = L"url:favicon:$DATA";
const char kDummyFaviconImageData[] =
    "\x42\x4D"          // Magic signature 'BM'
    "\x1E\x00\x00\x00"  // File size
    "\x00\x00\x00\x00"  // Reserved
    "\x1A\x00\x00\x00"  // Offset of the pixel data
    "\x0C\x00\x00\x00"  // Header Size
    "\x01\x00\x01\x00"  // Size: 1x1
    "\x01\x00"          // Reserved
    "\x18\x00"          // 24-bits
    "\x00\xFF\x00\x00"; // The pixel

struct FaviconGroup {
  const char16* favicon_url;
  const char16* site_url[2];
};

const FaviconGroup kIEFaviconGroup[2] = {
  {L"http://www.google.com/favicon.ico",
   {L"http://www.google.com/",
    L"http://www.subfolder.com/"}},
  {L"http://example.com/favicon.ico",
   {L"http://host:8080/cgi?q=query",
    L"http://chinese-title-favorite/"}},
};

bool CreateOrderBlob(const base::FilePath& favorites_folder,
                     const string16& path,
                     const std::vector<string16>& entries) {
  if (entries.size() > 255)
    return false;

  // Create a binary sequence for setting a specific order of favorites.
  // The format depends on the version of Shell32.dll, so we cannot embed
  // a binary constant here.
  std::vector<uint8> blob(20, 0);
  blob[16] = static_cast<uint8>(entries.size());

  for (size_t i = 0; i < entries.size(); ++i) {
    PIDLIST_ABSOLUTE id_list_full = ILCreateFromPath(
        favorites_folder.Append(path).Append(entries[i]).value().c_str());
    PUITEMID_CHILD id_list = ILFindLastID(id_list_full);
    // Include the trailing zero-length item id.  Don't include the single
    // element array.
    size_t id_list_size = id_list->mkid.cb + sizeof(id_list->mkid.cb);

    blob.resize(blob.size() + 8);
    uint32 total_size = id_list_size + 8;
    memcpy(&blob[blob.size() - 8], &total_size, 4);
    uint32 sort_index = i;
    memcpy(&blob[blob.size() - 4], &sort_index, 4);
    blob.resize(blob.size() + id_list_size);
    memcpy(&blob[blob.size() - id_list_size], id_list, id_list_size);
    ILFree(id_list_full);
  }

  string16 key_path = kIEFavoritesOrderKey;
  if (!path.empty())
    key_path += L"\\" + path;
  base::win::RegKey key;
  if (key.Create(HKEY_CURRENT_USER, key_path.c_str(), KEY_WRITE) !=
      ERROR_SUCCESS) {
    return false;
  }
  if (key.WriteValue(L"Order", &blob[0], blob.size(), REG_BINARY) !=
      ERROR_SUCCESS) {
    return false;
  }
  return true;
}

bool CreateUrlFileWithFavicon(const base::FilePath& file,
                              const std::wstring& url,
                              const std::wstring& favicon_url) {
  base::win::ScopedComPtr<IUniformResourceLocator> locator;
  HRESULT result = locator.CreateInstance(CLSID_InternetShortcut, NULL,
                                          CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return false;
  base::win::ScopedComPtr<IPersistFile> persist_file;
  result = persist_file.QueryFrom(locator);
  if (FAILED(result))
    return false;
  result = locator->SetURL(url.c_str(), 0);
  if (FAILED(result))
    return false;

  // Write favicon url if specified.
  if (!favicon_url.empty()) {
    base::win::ScopedComPtr<IPropertySetStorage> property_set_storage;
    if (FAILED(property_set_storage.QueryFrom(locator)))
      return false;
    base::win::ScopedComPtr<IPropertyStorage> property_storage;
    if (FAILED(property_set_storage->Open(FMTID_Intshcut,
                                          STGM_WRITE,
                                          property_storage.Receive()))) {
      return false;
    }
    PROPSPEC properties[] = {{PRSPEC_PROPID, PID_IS_ICONFILE}};
    // WriteMultiple takes an array of PROPVARIANTs, but since this code only
    // needs an array of size 1: a pointer to |pv_icon| is equivalent.
    base::win::ScopedPropVariant pv_icon;
    if (FAILED(InitPropVariantFromString(favicon_url.c_str(),
                                         pv_icon.Receive())) ||
        FAILED(property_storage->WriteMultiple(1, properties, &pv_icon, 0))) {
      return false;
    }
  }

  // Save the .url file.
  result = persist_file->Save(file.value().c_str(), TRUE);
  if (FAILED(result))
    return false;

  // Write dummy favicon image data in NTFS alternate data stream.
  return favicon_url.empty() || (file_util::WriteFile(
      file.ReplaceExtension(kFaviconStreamSuffix), kDummyFaviconImageData,
      sizeof kDummyFaviconImageData) != -1);
}

bool CreateUrlFile(const base::FilePath& file, const std::wstring& url) {
  return CreateUrlFileWithFavicon(file, url, std::wstring());
}

void ClearPStoreType(IPStore* pstore, const GUID* type, const GUID* subtype) {
  base::win::ScopedComPtr<IEnumPStoreItems, NULL> item;
  HRESULT result = pstore->EnumItems(0, type, subtype, 0, item.Receive());
  if (result == PST_E_OK) {
    char16* item_name;
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
    char16* name;
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

class TestObserver : public ProfileWriter,
                     public importer::ImporterProgressObserver {
 public:
  TestObserver() : ProfileWriter(NULL) {
    bookmark_count_ = 0;
    history_count_ = 0;
    password_count_ = 0;
    favicon_count_ = 0;
  }

  // importer::ImporterProgressObserver:
  virtual void ImportStarted() OVERRIDE {}
  virtual void ImportItemStarted(importer::ImportItem item) OVERRIDE {}
  virtual void ImportItemEnded(importer::ImportItem item) OVERRIDE {}
  virtual void ImportEnded() OVERRIDE {
    MessageLoop::current()->Quit();
    EXPECT_EQ(arraysize(kIEBookmarks), bookmark_count_);
    EXPECT_EQ(1, history_count_);
    EXPECT_EQ(arraysize(kIEFaviconGroup), favicon_count_);
  }

  virtual bool BookmarkModelIsLoaded() const {
    // Profile is ready for writing.
    return true;
  }

  virtual bool TemplateURLServiceIsLoaded() const {
    return true;
  }

  virtual void AddPasswordForm(const content::PasswordForm& form) {
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

  virtual void AddHistoryPage(const history::URLRows& page,
                              history::VisitSource visit_source) {
    // Importer should read the specified URL.
    for (size_t i = 0; i < page.size(); ++i) {
      if (page[i].title() == kIEIdentifyTitle &&
          page[i].url() == GURL(kIEIdentifyUrl))
        ++history_count_;
    }
    EXPECT_EQ(history::SOURCE_IE_IMPORTED, visit_source);
  }

  virtual void AddBookmarks(const std::vector<BookmarkEntry>& bookmarks,
                            const string16& top_level_folder_name) OVERRIDE {
    ASSERT_LE(bookmark_count_ + bookmarks.size(), arraysize(kIEBookmarks));
    // Importer should import the IE Favorites folder the same as the list,
    // in the same order.
    for (size_t i = 0; i < bookmarks.size(); ++i) {
      EXPECT_TRUE(EqualBookmarkEntry(bookmarks[i],
                                     kIEBookmarks[bookmark_count_]));
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

  virtual void AddFavicons(
      const std::vector<history::ImportedFaviconUsage>& usage) OVERRIDE {
    // Importer should group the favicon information for each favicon URL.
    for (size_t i = 0; i < arraysize(kIEFaviconGroup); ++i) {
      GURL favicon_url(kIEFaviconGroup[i].favicon_url);
      std::set<GURL> urls;
      for (size_t j = 0; j < arraysize(kIEFaviconGroup[i].site_url); ++j)
        urls.insert(GURL(kIEFaviconGroup[i].site_url[j]));

      SCOPED_TRACE(testing::Message() << "Expected Favicon: " << favicon_url);

      bool expected_favicon_url_found = false;
      for (size_t j = 0; j < usage.size(); ++j) {
        if (usage[j].favicon_url == favicon_url) {
          EXPECT_EQ(urls, usage[j].urls);
          expected_favicon_url_found = true;
          break;
        }
      }
      EXPECT_TRUE(expected_favicon_url_found);
    }

    favicon_count_ += usage.size();
  }

 private:
  ~TestObserver() {}

  size_t bookmark_count_;
  size_t history_count_;
  size_t password_count_;
  size_t favicon_count_;
};

class MalformedFavoritesRegistryTestObserver
    : public ProfileWriter,
      public importer::ImporterProgressObserver {
 public:
  MalformedFavoritesRegistryTestObserver() : ProfileWriter(NULL) {
    bookmark_count_ = 0;
  }

  // importer::ImporterProgressObserver:
  virtual void ImportStarted() OVERRIDE {}
  virtual void ImportItemStarted(importer::ImportItem item) OVERRIDE {}
  virtual void ImportItemEnded(importer::ImportItem item) OVERRIDE {}
  virtual void ImportEnded() OVERRIDE {
    MessageLoop::current()->Quit();
    EXPECT_EQ(arraysize(kIESortedBookmarks), bookmark_count_);
  }

  virtual bool BookmarkModelIsLoaded() const { return true; }
  virtual bool TemplateURLServiceIsLoaded() const { return true; }

  virtual void AddPasswordForm(const content::PasswordForm& form) {}
  virtual void AddHistoryPage(const history::URLRows& page,
                              history::VisitSource visit_source) {}
  virtual void AddKeyword(std::vector<TemplateURL*> template_url,
                          int default_keyword_index) {}
  virtual void AddBookmarks(const std::vector<BookmarkEntry>& bookmarks,
                            const string16& top_level_folder_name) OVERRIDE {
    ASSERT_LE(bookmark_count_ + bookmarks.size(),
              arraysize(kIESortedBookmarks));
    for (size_t i = 0; i < bookmarks.size(); ++i) {
      EXPECT_TRUE(EqualBookmarkEntry(bookmarks[i],
                                     kIESortedBookmarks[bookmark_count_]));
      ++bookmark_count_;
    }
  }

 private:
  ~MalformedFavoritesRegistryTestObserver() {}

  size_t bookmark_count_;
};

}  // namespace

class IEImporterTest : public ImporterTest {
 protected:
  virtual void SetUp() OVERRIDE {
    ImporterTest::SetUp();
    StartRegistryOverride();
  }

  virtual void TearDown() OVERRIDE {
    EndRegistryOverride();
  }

  void StartRegistryOverride() {
    EXPECT_EQ(ERROR_SUCCESS, RegOverridePredefKey(HKEY_CURRENT_USER, NULL));
    temp_hkcu_hive_key_.Create(HKEY_CURRENT_USER,
                               kUnitTestUserOverrideSubKey,
                               KEY_ALL_ACCESS);
    EXPECT_TRUE(temp_hkcu_hive_key_.Valid());
    EXPECT_EQ(ERROR_SUCCESS,
              RegOverridePredefKey(HKEY_CURRENT_USER,
                                   temp_hkcu_hive_key_.Handle()));
  }

  void EndRegistryOverride() {
    EXPECT_EQ(ERROR_SUCCESS, RegOverridePredefKey(HKEY_CURRENT_USER, NULL));
    temp_hkcu_hive_key_.Close();
    base::win::RegKey key(HKEY_CURRENT_USER, kUnitTestRegistrySubKey,
                          KEY_ALL_ACCESS);
    key.DeleteKey(L"");
  }

  base::win::RegKey temp_hkcu_hive_key_;
};

TEST_F(IEImporterTest, IEImporter) {
  // Sets up a favorites folder.
  base::win::ScopedCOMInitializer com_init;
  base::FilePath path = temp_dir_.path().AppendASCII("Favorites");
  CreateDirectory(path.value().c_str(), NULL);
  CreateDirectory(path.AppendASCII("SubFolder").value().c_str(), NULL);
  base::FilePath links_path = path.AppendASCII("Links");
  CreateDirectory(links_path.value().c_str(), NULL);
  CreateDirectory(links_path.AppendASCII("SubFolderOfLinks").value().c_str(),
                  NULL);
  CreateDirectory(path.AppendASCII("\x0061").value().c_str(), NULL);
  ASSERT_TRUE(CreateUrlFileWithFavicon(path.AppendASCII("Google Home Page.url"),
                                       L"http://www.google.com/",
                                       L"http://www.google.com/favicon.ico"));
  ASSERT_TRUE(CreateUrlFile(path.AppendASCII("SubFolder\\Title.url"),
                            L"http://www.link.com/"));
  ASSERT_TRUE(CreateUrlFileWithFavicon(path.AppendASCII("SubFolder.url"),
                                       L"http://www.subfolder.com/",
                                       L"http://www.google.com/favicon.ico"));
  ASSERT_TRUE(CreateUrlFile(path.AppendASCII("TheLink.url"),
                            L"http://www.links-thelink.com/"));
  ASSERT_TRUE(CreateUrlFileWithFavicon(path.AppendASCII("WithPortAndQuery.url"),
                                       L"http://host:8080/cgi?q=query",
                                       L"http://example.com/favicon.ico"));
  ASSERT_TRUE(CreateUrlFileWithFavicon(
      path.AppendASCII("\x0061").Append(L"\x4E2D\x6587.url"),
      L"http://chinese-title-favorite/",
      L"http://example.com/favicon.ico"));
  ASSERT_TRUE(CreateUrlFile(links_path.AppendASCII("TheLink.url"),
                            L"http://www.links-thelink.com/"));
  ASSERT_TRUE(CreateUrlFile(
      links_path.AppendASCII("SubFolderOfLinks").AppendASCII("SubLink.url"),
      L"http://www.links-sublink.com/"));
  ASSERT_TRUE(CreateUrlFile(path.AppendASCII("IEDefaultLink.url"),
                            L"http://go.microsoft.com/fwlink/?linkid=140813"));
  file_util::WriteFile(path.AppendASCII("InvalidUrlFile.url"), "x", 1);
  file_util::WriteFile(path.AppendASCII("PlainTextFile.txt"), "x", 1);

  const char16* root_links[] = {
    L"Links",
    L"Google Home Page.url",
    L"TheLink.url",
    L"SubFolder",
    L"WithPortAndQuery.url",
    L"a",
    L"SubFolder.url",
  };
  ASSERT_TRUE(CreateOrderBlob(
      base::FilePath(path), L"",
      std::vector<string16>(root_links, root_links + arraysize(root_links))));

  HRESULT res;

  // Sets up a special history link.
  base::win::ScopedComPtr<IUrlHistoryStg2> url_history_stg2;
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
  importer::SourceProfile source_profile;
  source_profile.importer_type = importer::TYPE_IE;
  source_profile.source_path = temp_dir_.path();

  // IUrlHistoryStg2::AddUrl seems to reset the override. Ensure it here.
  StartRegistryOverride();

  loop->PostTask(FROM_HERE, base::Bind(
      &ImporterHost::StartImportSettings,
      host.get(),
      source_profile,
      profile_.get(),
      importer::HISTORY | importer::PASSWORDS | importer::FAVORITES,
      observer,
      true));
  loop->Run();

  // Cleans up.
  url_history_stg2->DeleteUrl(kIEIdentifyUrl, 0);
  url_history_stg2.Release();
}

TEST_F(IEImporterTest, IEImporterMalformedFavoritesRegistry) {
  // Sets up a favorites folder.
  base::win::ScopedCOMInitializer com_init;
  base::FilePath path = temp_dir_.path().AppendASCII("Favorites");
  CreateDirectory(path.value().c_str(), NULL);
  CreateDirectory(path.AppendASCII("b").value().c_str(), NULL);
  ASSERT_TRUE(CreateUrlFile(path.AppendASCII("a.url"),
                            L"http://www.google.com/0"));
  ASSERT_TRUE(CreateUrlFile(path.AppendASCII("b").AppendASCII("a.url"),
                            L"http://www.google.com/1"));
  ASSERT_TRUE(CreateUrlFile(path.AppendASCII("b").AppendASCII("b.url"),
                            L"http://www.google.com/2"));
  ASSERT_TRUE(CreateUrlFile(path.AppendASCII("c.url"),
                            L"http://www.google.com/3"));

  struct BadBinaryData {
    const char* data;
    int length;
  };
  static const BadBinaryData kBadBinary[] = {
    // number_of_items field is truncated
    {"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
     "\x00\xff\xff\xff", 17},
    // number_of_items = 0xffff, but the byte sequence is too short.
    {"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
     "\xff\xff\x00\x00", 20},
    // number_of_items = 1, size_of_item is too big.
    {"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
     "\x01\x00\x00\x00"
     "\xff\xff\x00\x00\x00\x00\x00\x00"
     "\x00\x00\x00\x00", 32},
    // number_of_items = 1, size_of_item = 16, size_of_shid is too big.
    {"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
     "\x01\x00\x00\x00"
     "\x10\x00\x00\x00\x00\x00\x00\x00"
     "\xff\x7f\x00\x00" "\x00\x00\x00\x00", 36},
    // number_of_items = 1, size_of_item = 16, size_of_shid is too big.
    {"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
     "\x01\x00\x00\x00"
     "\x10\x00\x00\x00\x00\x00\x00\x00"
     "\x06\x00\x00\x00" "\x00\x00\x00\x00", 36},
  };

  // Verify malformed registry data are safely ignored and alphabetical
  // sort is performed.
  for (size_t i = 0; i < arraysize(kBadBinary); ++i) {
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS,
              key.Create(HKEY_CURRENT_USER, kIEFavoritesOrderKey, KEY_WRITE));
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(L"Order", kBadBinary[i].data, kBadBinary[i].length,
                             REG_BINARY));

    // Starts to import the above settings.
    MessageLoop* loop = MessageLoop::current();
    scoped_refptr<ImporterHost> host(new ImporterHost);

    MalformedFavoritesRegistryTestObserver* observer =
        new MalformedFavoritesRegistryTestObserver();
    host->SetObserver(observer);
    importer::SourceProfile source_profile;
    source_profile.importer_type = importer::TYPE_IE;
    source_profile.source_path = temp_dir_.path();

    loop->PostTask(FROM_HERE, base::Bind(
        &ImporterHost::StartImportSettings,
        host.get(),
        source_profile,
        profile_.get(),
        importer::FAVORITES,
        observer,
        true));
    loop->Run();
  }
}

TEST_F(IEImporterTest, IE7Importer) {
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

  string16 password;
  string16 username;
  ASSERT_TRUE(ie7_password::GetUserPassFromData(decrypted_data1, &username,
                                                &password));
  EXPECT_EQ(L"abcdefgh", username);
  EXPECT_EQ(L"abcdefghijkl", password);

  ASSERT_TRUE(ie7_password::GetUserPassFromData(decrypted_data2, &username,
                                                &password));
  EXPECT_EQ(L"abcdefghi", username);
  EXPECT_EQ(L"abcdefg", password);
}
