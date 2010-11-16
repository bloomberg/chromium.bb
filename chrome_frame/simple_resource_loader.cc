// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/simple_resource_loader.h"

#include <atlbase.h>

#include <algorithm>

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/i18n/rtl.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/i18n.h"
#include "base/win/windows_version.h"

#include "chrome_frame/policy_settings.h"

namespace {

const wchar_t kLocalesDirName[] = L"Locales";

bool IsInvalidTagCharacter(wchar_t tag_character) {
  return !(L'-' == tag_character ||
           IsAsciiDigit(tag_character) ||
           IsAsciiAlpha(tag_character));
}

// A helper function object that performs a lower-case ASCII comparison between
// two strings.
class CompareInsensitiveASCII
    : public std::unary_function<const std::wstring&, bool> {
 public:
  explicit CompareInsensitiveASCII(const std::wstring& value)
      : value_lowered_(WideToASCII(value)) {
    StringToLowerASCII(&value_lowered_);
  }
  bool operator()(const std::wstring& comparand) {
    return LowerCaseEqualsASCII(comparand, value_lowered_.c_str());
  }

 private:
  std::string value_lowered_;
};

// Returns true if the value was added.
bool PushBackIfAbsent(
    const std::wstring& value,
    std::vector<std::wstring>* collection) {
  if (collection->end() ==
      std::find_if(collection->begin(), collection->end(),
                   CompareInsensitiveASCII(value))) {
    collection->push_back(value);
    return true;
  }
  return false;
}

// Returns true if the collection is modified.
bool PushBackWithFallbackIfAbsent(
    const std::wstring& language,
    std::vector<std::wstring>* collection) {
  bool modified = false;

  if (!language.empty()) {
    // Try adding the language itself.
    modified = PushBackIfAbsent(language, collection);

    // Now try adding its fallback, if it has one.
    std::wstring::size_type dash_pos = language.find(L'-');
    if (0 < dash_pos && language.size() - 1 > dash_pos)
      modified |= PushBackIfAbsent(language.substr(0, dash_pos), collection);
  }

  return modified;
}

}  // namespace

SimpleResourceLoader::SimpleResourceLoader()
    : locale_dll_handle_(NULL) {
  // Find and load the resource DLL.
  std::vector<std::wstring> language_tags;

  // First, try the locale dictated by policy and its fallback.
  PushBackWithFallbackIfAbsent(Singleton<PolicySettings>()->ApplicationLocale(),
                               &language_tags);

  // Next, try the thread, process, user, system languages.
  GetPreferredLanguages(&language_tags);

  // Finally, fall-back on "en-US" (which may already be present in the vector,
  // but that's okay since we'll exit with success when the first is tried).
  language_tags.push_back(L"en-US");

  FilePath locales_path;
  FilePath locale_dll_path;

  DetermineLocalesDirectory(&locales_path);
  if (LoadLocaleDll(language_tags, locales_path, &locale_dll_handle_,
                    &locale_dll_path)) {
    language_ = locale_dll_path.BaseName().RemoveExtension().value();
  } else {
    NOTREACHED() << "Failed loading any resource dll (even \"en-US\").";
  }
}

SimpleResourceLoader::~SimpleResourceLoader() {
  locale_dll_handle_ = NULL;
}

// static
void SimpleResourceLoader::GetPreferredLanguages(
    std::vector<std::wstring>* language_tags) {
  DCHECK(language_tags);
  // The full set of preferred languages and their fallbacks are given priority.
  std::vector<std::wstring> languages;
  if (base::win::i18n::GetThreadPreferredUILanguageList(&languages)) {
    for (std::vector<std::wstring>::const_iterator scan = languages.begin(),
             end = languages.end(); scan != end; ++scan) {
      PushBackIfAbsent(*scan, language_tags);
    }
  }

  // Use the base i18n routines (i.e., ICU) as a last, best hope for something
  // meaningful for the user.
  PushBackWithFallbackIfAbsent(ASCIIToWide(base::i18n::GetConfiguredLocale()),
                               language_tags);
}

// static
void SimpleResourceLoader::DetermineLocalesDirectory(FilePath* locales_path) {
  DCHECK(locales_path);

  FilePath module_path;
  PathService::Get(base::DIR_MODULE, &module_path);
  *locales_path = module_path.Append(kLocalesDirName);

  // We may be residing in the "locales" directory's parent, or we might be
  // in a sibling directory. Move up one and look for Locales again in the
  // latter case.
  if (!file_util::DirectoryExists(*locales_path)) {
    *locales_path = module_path.DirName();
    *locales_path = locales_path->Append(kLocalesDirName);
  }

  // Don't make a second check to see if the dir is in the parent.  We'll notice
  // and log that in LoadLocaleDll when we actually try loading DLLs.
}

// static
bool SimpleResourceLoader::IsValidLanguageTag(
    const std::wstring& language_tag) {
  // "[a-zA-Z]+(-[a-zA-Z0-9]+)*" is a simplification, but better than nothing.
  // Rather than pick up the weight of a regex processor, just search for a
  // character that isn't in the above set.  This will at least weed out
  // attempts at "../../EvilBinary".
  return language_tag.end() == std::find_if(language_tag.begin(),
                                            language_tag.end(),
                                            &IsInvalidTagCharacter);
}

// static
bool SimpleResourceLoader::LoadLocaleDll(
    const std::vector<std::wstring>& language_tags,
    const FilePath& locales_path,
    HMODULE* dll_handle,
    FilePath* file_path) {
  DCHECK(file_path);

  // The dll should only have resources, not executable code.
  const DWORD load_flags =
      (base::win::GetVersion() >= base::win::VERSION_VISTA ?
          LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE :
          DONT_RESOLVE_DLL_REFERENCES);
  const std::wstring dll_suffix(L".dll");
  bool found_dll = false;

  for (std::vector<std::wstring>::const_iterator scan = language_tags.begin(),
         end = language_tags.end();
       scan != end;
       ++scan) {
    if (!IsValidLanguageTag(*scan)) {
      LOG(WARNING) << "Invalid language tag supplied while locating resources:"
                      " \"" << *scan << "\"";
      continue;
    }
    FilePath look_path = locales_path.Append(*scan + dll_suffix);
    HMODULE locale_dll_handle = LoadLibraryEx(look_path.value().c_str(), NULL,
                                              load_flags);
    if (NULL != locale_dll_handle) {
      *dll_handle = locale_dll_handle;
      *file_path = look_path;
      found_dll = true;
      break;
    }
    DPCHECK(ERROR_FILE_NOT_FOUND == GetLastError())
        << "Unable to load generated resources from " << look_path.value();
  }

  DCHECK(found_dll || file_util::DirectoryExists(locales_path))
      << "Could not locate locales DLL directory.";

  return found_dll;
}

std::wstring SimpleResourceLoader::GetLocalizedResource(int message_id) {
  if (!locale_dll_handle_) {
    DLOG(ERROR) << "locale resources are not loaded";
    return std::wstring();
  }

  DCHECK(IS_INTRESOURCE(message_id));

  const ATLSTRINGRESOURCEIMAGE* image = AtlGetStringResourceImage(
      locale_dll_handle_, message_id);
  if (!image) {
    // Fall back on the current module (shouldn't be any strings here except
    // in unittests).
    image = AtlGetStringResourceImage(_AtlBaseModule.GetModuleInstance(),
                                      message_id);
    if (!image) {
      NOTREACHED() << "unable to find resource: " << message_id;
      return std::wstring();
    }
  }
  return std::wstring(image->achString, image->nLength);
}

// static
std::wstring SimpleResourceLoader::GetLanguage() {
  return SimpleResourceLoader::instance()->language_;
}

// static
std::wstring SimpleResourceLoader::Get(int message_id) {
  SimpleResourceLoader* loader = SimpleResourceLoader::instance();
  return loader->GetLocalizedResource(message_id);
}

HMODULE SimpleResourceLoader::GetResourceModuleHandle() {
  return locale_dll_handle_;
}
