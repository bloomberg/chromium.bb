// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellcheck_host.h"

#include "base/string_split.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellcheck_host_impl.h"
#include "chrome/browser/spellchecker_platform_engine.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_common.h"
#include "content/common/notification_service.h"
#include "googleurl/src/gurl.h"
#include "third_party/hunspell/google/bdict.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

FilePath GetFirstChoiceFilePath(const std::string& language) {
  FilePath dict_dir;
  {
    // This should not do blocking IO from the UI thread!
    // Temporarily allow it for now.
    //   http://code.google.com/p/chromium/issues/detail?id=60643
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    PathService::Get(chrome::DIR_APP_DICTIONARIES, &dict_dir);
  }
  return SpellCheckCommon::GetVersionedFileName(language, dict_dir);
}

#if defined(OS_MACOSX)
// Collect metrics on how often Hunspell is used on OS X vs the native
// spellchecker.
void RecordSpellCheckStats(bool native_spellchecker_used,
                           const std::string& language) {
  static std::set<std::string> languages_seen;

  // Only count a language code once for each session..
  if (languages_seen.find(language) != languages_seen.end()) {
    return;
  }
  languages_seen.insert(language);

  enum {
    SPELLCHECK_OSX_NATIVE_SPELLCHECKER_USED = 0,
    SPELLCHECK_HUNSPELL_USED = 1
  };

  bool engine_used = native_spellchecker_used ?
                         SPELLCHECK_OSX_NATIVE_SPELLCHECKER_USED :
                         SPELLCHECK_HUNSPELL_USED;

  UMA_HISTOGRAM_COUNTS("SpellCheck.OSXEngineUsed", engine_used);
}
#endif

#if defined(OS_WIN)
FilePath GetFallbackFilePath(const FilePath& first_choice) {
  FilePath dict_dir;
  PathService::Get(chrome::DIR_USER_DATA, &dict_dir);
  return dict_dir.Append(first_choice.BaseName());
}
#endif

}  // namespace

// Constructed on UI thread.
SpellCheckHost::SpellCheckHost(SpellCheckHostObserver* observer,
                               const std::string& language,
                               URLRequestContextGetter* request_context_getter)
    : observer_(observer),
      language_(language),
      file_(base::kInvalidPlatformFileValue),
      tried_to_download_(false),
      use_platform_spellchecker_(false),
      request_context_getter_(request_context_getter) {
  DCHECK(observer_);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FilePath personal_file_directory;
  PathService::Get(chrome::DIR_USER_DATA, &personal_file_directory);
  custom_dictionary_file_ =
      personal_file_directory.Append(chrome::kCustomDictionaryFileName);

  bdict_file_path_ = GetFirstChoiceFilePath(language);
}

SpellCheckHost::~SpellCheckHost() {
  if (file_ != base::kInvalidPlatformFileValue)
    base::ClosePlatformFile(file_);
}

void SpellCheckHost::Initialize() {
  if (SpellCheckerPlatform::SpellCheckerAvailable() &&
      SpellCheckerPlatform::PlatformSupportsLanguage(language_)) {
#if defined(OS_MACOSX)
    RecordSpellCheckStats(true, language_);
#endif
    use_platform_spellchecker_ = true;
    SpellCheckerPlatform::SetLanguage(language_);
    MessageLoop::current()->PostTask(FROM_HERE,
        NewRunnableMethod(this,
            &SpellCheckHost::InformObserverOfInitialization));
    return;
  }
>>>>>>> Update a bunch of files to the new location of notification files.

// static
scoped_refptr<SpellCheckHost> SpellCheckHost::Create(
    SpellCheckHostObserver* observer,
    const std::string& language,
    URLRequestContextGetter* request_context_getter) {
  scoped_refptr<SpellCheckHostImpl> host =
      new SpellCheckHostImpl(observer,
                             language,
                             request_context_getter);
  if (!host)
    return NULL;

  host->Initialize();
  return host;
}

// static
int SpellCheckHost::GetSpellCheckLanguages(
    Profile* profile,
    std::vector<std::string>* languages) {
  StringPrefMember accept_languages_pref;
  StringPrefMember dictionary_language_pref;
  accept_languages_pref.Init(prefs::kAcceptLanguages, profile->GetPrefs(),
                             NULL);
  dictionary_language_pref.Init(prefs::kSpellCheckDictionary,
                                profile->GetPrefs(), NULL);
  std::string dictionary_language = dictionary_language_pref.GetValue();

  // The current dictionary language should be there.
  languages->push_back(dictionary_language);

  // Now scan through the list of accept languages, and find possible mappings
  // from this list to the existing list of spell check languages.
  std::vector<std::string> accept_languages;

  if (SpellCheckerPlatform::SpellCheckerAvailable())
    SpellCheckerPlatform::GetAvailableLanguages(&accept_languages);
  else
    base::SplitString(accept_languages_pref.GetValue(), ',', &accept_languages);

  for (std::vector<std::string>::const_iterator i = accept_languages.begin();
       i != accept_languages.end(); ++i) {
    std::string language =
        SpellCheckCommon::GetCorrespondingSpellCheckLanguage(*i);
    if (!language.empty() &&
        std::find(languages->begin(), languages->end(), language) ==
        languages->end()) {
      languages->push_back(language);
    }
  }

  for (size_t i = 0; i < languages->size(); ++i) {
    if ((*languages)[i] == dictionary_language)
      return i;
  }
  return -1;
}

