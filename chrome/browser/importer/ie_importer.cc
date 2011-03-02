// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/ie_importer.h"

#include <ole2.h>
#include <intshcut.h>
#include <pstore.h>
#include <shlobj.h>
#include <urlhist.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "app/win/scoped_co_mem.h"
#include "app/win/scoped_com_initializer.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_comptr_win.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/values.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/password_manager/ie7_password.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/password_form.h"

using base::Time;
using webkit_glue::PasswordForm;

namespace {

// Gets the creation time of the given file or directory.
static Time GetFileCreationTime(const std::wstring& file) {
  Time creation_time;
  base::win::ScopedHandle file_handle(
      CreateFile(file.c_str(), GENERIC_READ,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                 NULL, OPEN_EXISTING,
                 FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, NULL));
  FILETIME creation_filetime;
  if (GetFileTime(file_handle, &creation_filetime, NULL, NULL))
    creation_time = Time::FromFileTime(creation_filetime);
  return creation_time;
}

}  // namespace

// static
// {E161255A-37C3-11D2-BCAA-00C04fD929DB}
const GUID IEImporter::kPStoreAutocompleteGUID = {0xe161255a, 0x37c3, 0x11d2,
    {0xbc, 0xaa, 0x00, 0xc0, 0x4f, 0xd9, 0x29, 0xdb}};
// {A79029D6-753E-4e27-B807-3D46AB1545DF}
const GUID IEImporter::kUnittestGUID = { 0xa79029d6, 0x753e, 0x4e27,
    {0xb8, 0x7, 0x3d, 0x46, 0xab, 0x15, 0x45, 0xdf}};

IEImporter::IEImporter() {
}

void IEImporter::StartImport(const importer::ProfileInfo& profile_info,
                             uint16 items,
                             ImporterBridge* bridge) {
  bridge_ = bridge;
  source_path_ = profile_info.source_path.value();

  bridge_->NotifyStarted();

  // Some IE settings (such as Protected Storage) are obtained via COM APIs.
  app::win::ScopedCOMInitializer com_initializer;

  if ((items & importer::HOME_PAGE) && !cancelled())
    ImportHomepage();  // Doesn't have a UI item.
  // The order here is important!
  if ((items & importer::HISTORY) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::HISTORY);
    ImportHistory();
    bridge_->NotifyItemEnded(importer::HISTORY);
  }
  if ((items & importer::FAVORITES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::FAVORITES);
    ImportFavorites();
    bridge_->NotifyItemEnded(importer::FAVORITES);
  }
  if ((items & importer::SEARCH_ENGINES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::SEARCH_ENGINES);
    ImportSearchEngines();
    bridge_->NotifyItemEnded(importer::SEARCH_ENGINES);
  }
  if ((items & importer::PASSWORDS) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::PASSWORDS);
    // Always import IE6 passwords.
    ImportPasswordsIE6();

    if (CurrentIEVersion() >= 7)
      ImportPasswordsIE7();
    bridge_->NotifyItemEnded(importer::PASSWORDS);
  }
  bridge_->NotifyEnded();
}

IEImporter::~IEImporter() {
}

void IEImporter::ImportFavorites() {
  std::wstring path;

  FavoritesInfo info;
  if (!GetFavoritesInfo(&info))
    return;

  BookmarkVector bookmarks;
  ParseFavoritesFolder(info, &bookmarks);

  if (!bookmarks.empty() && !cancelled()) {
    const std::wstring& first_folder_name =
        UTF16ToWide(l10n_util::GetStringUTF16(IDS_BOOKMARK_GROUP_FROM_IE));
    int options = 0;
    if (import_to_bookmark_bar())
      options = ProfileWriter::IMPORT_TO_BOOKMARK_BAR;
    bridge_->AddBookmarkEntries(bookmarks, first_folder_name, options);
  }
}

void IEImporter::ImportPasswordsIE6() {
  GUID AutocompleteGUID = kPStoreAutocompleteGUID;
  if (!source_path_.empty()) {
    // We supply a fake GUID for testting.
    AutocompleteGUID = kUnittestGUID;
  }

  // The PStoreCreateInstance function retrieves an interface pointer
  // to a storage provider. But this function has no associated import
  // library or header file, we must call it using the LoadLibrary()
  // and GetProcAddress() functions.
  typedef HRESULT (WINAPI *PStoreCreateFunc)(IPStore**, DWORD, DWORD, DWORD);
  HMODULE pstorec_dll = LoadLibrary(L"pstorec.dll");
  if (!pstorec_dll)
    return;
  PStoreCreateFunc PStoreCreateInstance =
      (PStoreCreateFunc)GetProcAddress(pstorec_dll, "PStoreCreateInstance");
  if (!PStoreCreateInstance) {
    FreeLibrary(pstorec_dll);
    return;
  }

  ScopedComPtr<IPStore, &IID_IPStore> pstore;
  HRESULT result = PStoreCreateInstance(pstore.Receive(), 0, 0, 0);
  if (result != S_OK) {
    FreeLibrary(pstorec_dll);
    return;
  }

  std::vector<AutoCompleteInfo> ac_list;

  // Enumerates AutoComplete items in the protected database.
  ScopedComPtr<IEnumPStoreItems, &IID_IEnumPStoreItems> item;
  result = pstore->EnumItems(0, &AutocompleteGUID,
                             &AutocompleteGUID, 0, item.Receive());
  if (result != PST_E_OK) {
    pstore.Release();
    FreeLibrary(pstorec_dll);
    return;
  }

  wchar_t* item_name;
  while (!cancelled() && SUCCEEDED(item->Next(1, &item_name, 0))) {
    DWORD length = 0;
    unsigned char* buffer = NULL;
    result = pstore->ReadItem(0, &AutocompleteGUID, &AutocompleteGUID,
                              item_name, &length, &buffer, NULL, 0);
    if (SUCCEEDED(result)) {
      AutoCompleteInfo ac;
      ac.key = item_name;
      std::wstring data;
      data.insert(0, reinterpret_cast<wchar_t*>(buffer),
                  length / sizeof(wchar_t));

      // The key name is always ended with ":StringData".
      const wchar_t kDataSuffix[] = L":StringData";
      size_t i = ac.key.rfind(kDataSuffix);
      if (i != std::wstring::npos && ac.key.substr(i) == kDataSuffix) {
        ac.key.erase(i);
        ac.is_url = (ac.key.find(L"://") != std::wstring::npos);
        ac_list.push_back(ac);
        base::SplitString(data, L'\0', &ac_list[ac_list.size() - 1].data);
      }
      CoTaskMemFree(buffer);
    }
    CoTaskMemFree(item_name);
  }
  // Releases them before unload the dll.
  item.Release();
  pstore.Release();
  FreeLibrary(pstorec_dll);

  size_t i;
  for (i = 0; i < ac_list.size(); i++) {
    if (!ac_list[i].is_url || ac_list[i].data.size() < 2)
      continue;

    GURL url(ac_list[i].key.c_str());
    if (!(LowerCaseEqualsASCII(url.scheme(), chrome::kHttpScheme) ||
        LowerCaseEqualsASCII(url.scheme(), chrome::kHttpsScheme))) {
      continue;
    }

    PasswordForm form;
    GURL::Replacements rp;
    rp.ClearUsername();
    rp.ClearPassword();
    rp.ClearQuery();
    rp.ClearRef();
    form.origin = url.ReplaceComponents(rp);
    form.username_value = ac_list[i].data[0];
    form.password_value = ac_list[i].data[1];
    form.signon_realm = url.GetOrigin().spec();

    // This is not precise, because a scheme of https does not imply a valid
    // certificate was presented; however we assign it this way so that if we
    // import a password from IE whose scheme is https, we give it the benefit
    // of the doubt and DONT auto-fill it unless the form appears under
    // valid SSL conditions.
    form.ssl_valid = url.SchemeIsSecure();

    // Goes through the list to find out the username field
    // of the web page.
    size_t list_it, item_it;
    for (list_it = 0; list_it < ac_list.size(); ++list_it) {
      if (ac_list[list_it].is_url)
        continue;

      for (item_it = 0; item_it < ac_list[list_it].data.size(); ++item_it)
        if (ac_list[list_it].data[item_it] == form.username_value) {
          form.username_element = ac_list[list_it].key;
          break;
        }
    }

    bridge_->SetPasswordForm(form);
  }
}

void IEImporter::ImportPasswordsIE7() {
  if (!source_path_.empty()) {
    // We have been called from the unit tests. Don't import real passwords.
    return;
  }

  const wchar_t kStorage2Path[] =
      L"Software\\Microsoft\\Internet Explorer\\IntelliForms\\Storage2";

  base::win::RegKey key(HKEY_CURRENT_USER, kStorage2Path, KEY_READ);
  base::win::RegistryValueIterator reg_iterator(HKEY_CURRENT_USER,
                                                kStorage2Path);
  IE7PasswordInfo password_info;
  while (reg_iterator.Valid() && !cancelled()) {
    // Get the size of the encrypted data.
    DWORD value_len = 0;
    key.ReadValue(reg_iterator.Name(), NULL, &value_len, NULL);
    if (value_len) {
      // Query the encrypted data.
      password_info.encrypted_data.resize(value_len);
      if (key.ReadValue(reg_iterator.Name(),
                        &password_info.encrypted_data.front(),
                        &value_len, NULL) == ERROR_SUCCESS) {
        password_info.url_hash = reg_iterator.Name();
        password_info.date_created = Time::Now();

        bridge_->AddIE7PasswordInfo(password_info);
      }
    }

    ++reg_iterator;
  }
}

// Reads history information from COM interface.
void IEImporter::ImportHistory() {
  const std::string kSchemes[] = {chrome::kHttpScheme,
                                  chrome::kHttpsScheme,
                                  chrome::kFtpScheme,
                                  chrome::kFileScheme};
  int total_schemes = arraysize(kSchemes);

  ScopedComPtr<IUrlHistoryStg2> url_history_stg2;
  HRESULT result;
  result = url_history_stg2.CreateInstance(CLSID_CUrlHistory, NULL,
                                           CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return;
  ScopedComPtr<IEnumSTATURL> enum_url;
  if (SUCCEEDED(result = url_history_stg2->EnumUrls(enum_url.Receive()))) {
    std::vector<history::URLRow> rows;
    STATURL stat_url;
    ULONG fetched;
    while (!cancelled() &&
           (result = enum_url->Next(1, &stat_url, &fetched)) == S_OK) {
      std::wstring url_string;
      std::wstring title_string;
      if (stat_url.pwcsUrl) {
        url_string = stat_url.pwcsUrl;
        CoTaskMemFree(stat_url.pwcsUrl);
      }
      if (stat_url.pwcsTitle) {
        title_string = stat_url.pwcsTitle;
        CoTaskMemFree(stat_url.pwcsTitle);
      }

      GURL url(url_string);
      // Skips the URLs that are invalid or have other schemes.
      if (!url.is_valid() ||
          (std::find(kSchemes, kSchemes + total_schemes, url.scheme()) ==
           kSchemes + total_schemes))
        continue;

      history::URLRow row(url);
      row.set_title(title_string);
      row.set_last_visit(Time::FromFileTime(stat_url.ftLastVisited));
      if (stat_url.dwFlags == STATURL_QUERYFLAG_TOPLEVEL) {
        row.set_visit_count(1);
        row.set_hidden(false);
      } else {
        row.set_hidden(true);
      }

      rows.push_back(row);
    }

    if (!rows.empty() && !cancelled()) {
      bridge_->SetHistoryItems(rows, history::SOURCE_IE_IMPORTED);
    }
  }
}

void IEImporter::ImportSearchEngines() {
  // On IE, search engines are stored in the registry, under:
  // Software\Microsoft\Internet Explorer\SearchScopes
  // Each key represents a search engine. The URL value contains the URL and
  // the DisplayName the name.
  // The default key's name is contained under DefaultScope.
  const wchar_t kSearchScopePath[] =
      L"Software\\Microsoft\\Internet Explorer\\SearchScopes";

  base::win::RegKey key(HKEY_CURRENT_USER, kSearchScopePath, KEY_READ);
  std::wstring default_search_engine_name;
  const TemplateURL* default_search_engine = NULL;
  std::map<std::string, TemplateURL*> search_engines_map;
  key.ReadValue(L"DefaultScope", &default_search_engine_name);
  base::win::RegistryKeyIterator key_iterator(HKEY_CURRENT_USER,
                                              kSearchScopePath);
  while (key_iterator.Valid()) {
    std::wstring sub_key_name = kSearchScopePath;
    sub_key_name.append(L"\\").append(key_iterator.Name());
    base::win::RegKey sub_key(HKEY_CURRENT_USER, sub_key_name.c_str(),
                              KEY_READ);
    std::wstring wide_url;
    if ((sub_key.ReadValue(L"URL", &wide_url) != ERROR_SUCCESS) ||
        wide_url.empty()) {
      VLOG(1) << "No URL for IE search engine at " << key_iterator.Name();
      ++key_iterator;
      continue;
    }
    // For the name, we try the default value first (as Live Search uses a
    // non displayable name in DisplayName, and the readable name under the
    // default value).
    std::wstring name;
    if ((sub_key.ReadValue(NULL, &name) != ERROR_SUCCESS) || name.empty()) {
      // Try the displayable name.
      if ((sub_key.ReadValue(L"DisplayName", &name) != ERROR_SUCCESS) ||
          name.empty()) {
        VLOG(1) << "No name for IE search engine at " << key_iterator.Name();
        ++key_iterator;
        continue;
      }
    }

    std::string url(WideToUTF8(wide_url));
    std::map<std::string, TemplateURL*>::iterator t_iter =
        search_engines_map.find(url);
    TemplateURL* template_url =
        (t_iter != search_engines_map.end()) ? t_iter->second : NULL;
    if (!template_url) {
      // First time we see that URL.
      template_url = new TemplateURL();
      template_url->set_short_name(name);
      template_url->SetURL(url, 0, 0);
      // Give this a keyword to facilitate tab-to-search, if possible.
      GURL gurl = GURL(url);
      template_url->set_keyword(TemplateURLModel::GenerateKeyword(gurl,
                                                                  false));
      template_url->set_logo_id(
          TemplateURLPrepopulateData::GetSearchEngineLogo(gurl));
      template_url->set_show_in_default_list(true);
      search_engines_map[url] = template_url;
    }
    if (template_url && key_iterator.Name() == default_search_engine_name) {
      DCHECK(!default_search_engine);
      default_search_engine = template_url;
    }
    ++key_iterator;
  }

  // ProfileWriter::AddKeywords() requires a vector and we have a map.
  std::map<std::string, TemplateURL*>::iterator t_iter;
  std::vector<TemplateURL*> search_engines;
  int default_search_engine_index = -1;
  for (t_iter = search_engines_map.begin(); t_iter != search_engines_map.end();
       ++t_iter) {
    search_engines.push_back(t_iter->second);
    if (default_search_engine == t_iter->second) {
      default_search_engine_index =
          static_cast<int>(search_engines.size()) - 1;
    }
  }
  bridge_->SetKeywords(search_engines, default_search_engine_index, true);
}

void IEImporter::ImportHomepage() {
  const wchar_t kIESettingsMain[] =
      L"Software\\Microsoft\\Internet Explorer\\Main";
  const wchar_t kIEHomepage[] = L"Start Page";
  const wchar_t kIEDefaultHomepage[] = L"Default_Page_URL";

  base::win::RegKey key(HKEY_CURRENT_USER, kIESettingsMain, KEY_READ);
  std::wstring homepage_url;
  if (key.ReadValue(kIEHomepage, &homepage_url) != ERROR_SUCCESS ||
      homepage_url.empty())
    return;

  GURL homepage = GURL(homepage_url);
  if (!homepage.is_valid())
    return;

  // Check to see if this is the default website and skip import.
  base::win::RegKey keyDefault(HKEY_LOCAL_MACHINE, kIESettingsMain, KEY_READ);
  std::wstring default_homepage_url;
  LONG result = keyDefault.ReadValue(kIEDefaultHomepage, &default_homepage_url);
  if (result == ERROR_SUCCESS && !default_homepage_url.empty()) {
    if (homepage.spec() == GURL(default_homepage_url).spec())
      return;
  }

  bridge_->AddHomePage(homepage);
}

bool IEImporter::GetFavoritesInfo(IEImporter::FavoritesInfo *info) {
  if (!source_path_.empty()) {
    // Source path exists during testing.
    info->path = source_path_;
    file_util::AppendToPath(&info->path, L"Favorites");
    info->links_folder = L"Links";
    return true;
  }

  // IE stores the favorites in the Favorites under user profile's folder.
  wchar_t buffer[MAX_PATH];
  if (FAILED(SHGetFolderPath(NULL, CSIDL_FAVORITES, NULL,
                             SHGFP_TYPE_CURRENT, buffer)))
    return false;
  info->path = buffer;

  // There is a Links folder under Favorites folder in Windows Vista, but it
  // is not recording in Vista's registry. So in Vista, we assume the Links
  // folder is under Favorites folder since it looks like there is not name
  // different in every language version of Windows Vista.
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    // The Link folder name is stored in the registry.
    DWORD buffer_length = sizeof(buffer);
    base::win::RegKey reg_key(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Internet Explorer\\Toolbar", KEY_READ);
    if (reg_key.ReadValue(L"LinksFolderName", buffer,
                          &buffer_length, NULL) != ERROR_SUCCESS)
      return false;
    info->links_folder = buffer;
  } else {
    info->links_folder = L"Links";
  }

  return true;
}

void IEImporter::ParseFavoritesFolder(const FavoritesInfo& info,
                                      BookmarkVector* bookmarks) {
  std::wstring ie_folder =
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_BOOKMARK_GROUP_FROM_IE));
  BookmarkVector toolbar_bookmarks;
  FilePath file;
  std::vector<FilePath::StringType> file_list;
  FilePath favorites_path(info.path);
  // Favorites path length.  Make sure it doesn't include the trailing \.
  size_t favorites_path_len =
      favorites_path.StripTrailingSeparators().value().size();
  file_util::FileEnumerator file_enumerator(
      favorites_path, true, file_util::FileEnumerator::FILES);
  while (!(file = file_enumerator.Next()).value().empty() && !cancelled())
    file_list.push_back(file.value());

  // Keep the bookmarks in alphabetical order.
  std::sort(file_list.begin(), file_list.end());

  for (std::vector<FilePath::StringType>::iterator it = file_list.begin();
       it != file_list.end(); ++it) {
    FilePath shortcut(*it);
    if (!LowerCaseEqualsASCII(shortcut.Extension(), ".url"))
      continue;

    // Skip the bookmark with invalid URL.
    GURL url = GURL(ResolveInternetShortcut(*it));
    if (!url.is_valid())
      continue;

    // Make the relative path from the Favorites folder, without the basename.
    // ex. Suppose that the Favorites folder is C:\Users\Foo\Favorites.
    //   C:\Users\Foo\Favorites\Foo.url -> ""
    //   C:\Users\Foo\Favorites\Links\Bar\Baz.url -> "Links\Bar"
    FilePath::StringType relative_string =
        shortcut.DirName().value().substr(favorites_path_len);
    if (relative_string.size() > 0 && FilePath::IsSeparator(relative_string[0]))
      relative_string = relative_string.substr(1);
    FilePath relative_path(relative_string);

    ProfileWriter::BookmarkEntry entry;
    // Remove the dot, the file extension, and the directory path.
    entry.title = shortcut.RemoveExtension().BaseName().value();
    entry.url = url;
    entry.creation_time = GetFileCreationTime(*it);
    if (!relative_path.empty())
      relative_path.GetComponents(&entry.path);

    // Flatten the bookmarks in Link folder onto bookmark toolbar. Otherwise,
    // put it into "Other bookmarks".
    if (import_to_bookmark_bar() &&
        (entry.path.size() > 0 && entry.path[0] == info.links_folder)) {
      entry.in_toolbar = true;
      entry.path.erase(entry.path.begin());
      toolbar_bookmarks.push_back(entry);
    } else {
      // We put the bookmarks in a "Imported From IE"
      // folder, so that we don't mess up the "Other bookmarks".
      if (!import_to_bookmark_bar())
        entry.path.insert(entry.path.begin(), ie_folder);
      bookmarks->push_back(entry);
    }
  }
  bookmarks->insert(bookmarks->begin(), toolbar_bookmarks.begin(),
                    toolbar_bookmarks.end());
}

std::wstring IEImporter::ResolveInternetShortcut(const std::wstring& file) {
  app::win::ScopedCoMem<wchar_t> url;
  ScopedComPtr<IUniformResourceLocator> url_locator;
  HRESULT result = url_locator.CreateInstance(CLSID_InternetShortcut, NULL,
                                              CLSCTX_INPROC_SERVER);
  if (FAILED(result))
    return std::wstring();

  ScopedComPtr<IPersistFile> persist_file;
  result = persist_file.QueryFrom(url_locator);
  if (FAILED(result))
    return std::wstring();

  // Loads the Internet Shortcut from persistent storage.
  result = persist_file->Load(file.c_str(), STGM_READ);
  if (FAILED(result))
    return std::wstring();

  result = url_locator->GetURL(&url);
  // GetURL can return S_FALSE (FAILED(S_FALSE) is false) when url == NULL.
  if (FAILED(result) || (url == NULL))
    return std::wstring();

  return std::wstring(url);
}

int IEImporter::CurrentIEVersion() const {
  static int version = -1;
  if (version < 0) {
    wchar_t buffer[128];
    DWORD buffer_length = sizeof(buffer);
    base::win::RegKey reg_key(HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Internet Explorer", KEY_READ);
    LONG result = reg_key.ReadValue(L"Version", buffer, &buffer_length, NULL);
    version = ((result == ERROR_SUCCESS)? _wtoi(buffer) : 0);
  }
  return version;
}
