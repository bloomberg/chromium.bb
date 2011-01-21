// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellcheck_host.h"

#include <fcntl.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_split.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellcheck_host_observer.h"
#include "chrome/browser/spellchecker_platform_engine.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_common.h"
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
    use_platform_spellchecker_ = true;
    SpellCheckerPlatform::SetLanguage(language_);
    MessageLoop::current()->PostTask(FROM_HERE,
        NewRunnableMethod(this,
            &SpellCheckHost::InformObserverOfInitialization));
    return;
  }

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &SpellCheckHost::InitializeDictionaryLocation));
}

void SpellCheckHost::UnsetObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  observer_ = NULL;
  request_context_getter_ = NULL;
  fetcher_.reset();
}

void SpellCheckHost::AddWord(const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  custom_words_.push_back(word);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &SpellCheckHost::WriteWordToCustomDictionary, word));
  NotificationService::current()->Notify(
      NotificationType::SPELLCHECK_WORD_ADDED,
      Source<SpellCheckHost>(this), NotificationService::NoDetails());
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

void SpellCheckHost::InitializeDictionaryLocation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

#if defined(OS_WIN)
  // Check if the dictionary exists in the fallback location. If so, use it
  // rather than downloading anew.
  FilePath fallback = GetFallbackFilePath(bdict_file_path_);
  if (!file_util::PathExists(bdict_file_path_) &&
      file_util::PathExists(fallback)) {
    bdict_file_path_ = fallback;
  }
#endif

  InitializeInternal();
}

void SpellCheckHost::InitializeInternal() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!observer_)
    return;

  file_ = base::CreatePlatformFile(
      bdict_file_path_,
      base::PLATFORM_FILE_READ | base::PLATFORM_FILE_OPEN,
      NULL, NULL);

  // File didn't exist. Download it.
  if (file_ == base::kInvalidPlatformFileValue && !tried_to_download_ &&
      request_context_getter_) {
    // We download from the ui thread because we need to know that
    // |request_context_getter_| is still valid before initiating the download.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &SpellCheckHost::DownloadDictionary));
    return;
  }

  request_context_getter_ = NULL;

  if (file_ != base::kInvalidPlatformFileValue) {
    // Load custom dictionary.
    std::string contents;
    file_util::ReadFileToString(custom_dictionary_file_, &contents);
    std::vector<std::string> list_of_words;
    base::SplitString(contents, '\n', &list_of_words);
    for (size_t i = 0; i < list_of_words.size(); ++i)
      custom_words_.push_back(list_of_words[i]);
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
          &SpellCheckHost::InformObserverOfInitialization));
}

void SpellCheckHost::InitializeOnFileThread() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::FILE));

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &SpellCheckHost::Initialize));
}

void SpellCheckHost::InformObserverOfInitialization() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (observer_)
    observer_->SpellCheckHostInitialized();
}

void SpellCheckHost::DownloadDictionary() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!request_context_getter_) {
    InitializeOnFileThread();
    return;
  }

  // Determine URL of file to download.
  static const char kDownloadServerUrl[] =
      "http://cache.pack.google.com/edgedl/chrome/dict/";
  GURL url = GURL(std::string(kDownloadServerUrl) +
      StringToLowerASCII(WideToUTF8(
          bdict_file_path_.BaseName().ToWStringHack())));
  fetcher_.reset(new URLFetcher(url, URLFetcher::GET, this));
  fetcher_->set_request_context(request_context_getter_);
  tried_to_download_ = true;
  fetcher_->Start();
  request_context_getter_ = NULL;
}

void SpellCheckHost::WriteWordToCustomDictionary(const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Stored in UTF-8.
  std::string word_to_add(word + "\n");
  FILE* f = file_util::OpenFile(custom_dictionary_file_, "a+");
  if (f != NULL)
    fputs(word_to_add.c_str(), f);
  file_util::CloseFile(f);
}

void SpellCheckHost::OnURLFetchComplete(const URLFetcher* source,
                                        const GURL& url,
                                        const net::URLRequestStatus& status,
                                        int response_code,
                                        const ResponseCookies& cookies,
                                        const std::string& data) {
  DCHECK(source);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  fetcher_.reset();

  if ((response_code / 100) != 2) {
    // Initialize will not try to download the file a second time.
    LOG(ERROR) << "Failure to download dictionary.";
    InitializeOnFileThread();
    return;
  }

  // Basic sanity check on the dictionary.
  // There's the small chance that we might see a 200 status code for a body
  // that represents some form of failure.
  if (data.size() < 4 || data[0] != 'B' || data[1] != 'D' || data[2] != 'i' ||
      data[3] != 'c') {
    LOG(ERROR) << "Failure to download dictionary.";
    InitializeOnFileThread();
    return;
  }

  data_ = data;
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &SpellCheckHost::SaveDictionaryData));
}

void SpellCheckHost::SaveDictionaryData() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // To prevent corrupted dictionary data from causing a renderer crash, scan
  // the dictionary data and verify it is sane before save it to a file.
  if (!hunspell::BDict::Verify(data_.data(), data_.size())) {
    LOG(ERROR) << "Failure to verify the downloaded dictionary.";
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this,
                          &SpellCheckHost::InformObserverOfInitialization));
    return;
  }

  size_t bytes_written =
      file_util::WriteFile(bdict_file_path_, data_.data(), data_.length());
  if (bytes_written != data_.length()) {
    bool success = false;
#if defined(OS_WIN)
    bdict_file_path_ = GetFallbackFilePath(bdict_file_path_);
    bytes_written =
        file_util::WriteFile(GetFallbackFilePath(bdict_file_path_),
                                                 data_.data(), data_.length());
    if (bytes_written == data_.length())
      success = true;
#endif
    data_.clear();

    if (!success) {
      LOG(ERROR) << "Failure to save dictionary.";
      file_util::Delete(bdict_file_path_, false);
      // To avoid trying to load a partially saved dictionary, shortcut the
      // Initialize() call.
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this,
              &SpellCheckHost::InformObserverOfInitialization));
      return;
    }
  }

  data_.clear();
  Initialize();
}
