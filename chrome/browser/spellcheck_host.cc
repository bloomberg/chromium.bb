// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellcheck_host.h"

#include <fcntl.h>

#include "app/l10n_util.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/notification_service.h"
#include "googleurl/src/gurl.h"

namespace {

static const struct {
  // The language.
  const char* language;

  // The corresponding language and region, used by the dictionaries.
  const char* language_region;
} g_supported_spellchecker_languages[] = {
  // Several languages are not to be included in the spellchecker list:
  //   th-TH, hu-HU, bg-BG, uk-UA
  {"ca", "ca-ES"},
  {"cs", "cs-CZ"},
  {"da", "da-DK"},
  {"de", "de-DE"},
  {"el", "el-GR"},
  {"en-AU", "en-AU"},
  {"en-GB", "en-GB"},
  {"en-US", "en-US"},
  {"es", "es-ES"},
  {"et", "et-EE"},
  {"fr", "fr-FR"},
  {"he", "he-IL"},
  {"hi", "hi-IN"},
  {"hr", "hr-HR"},
  {"id", "id-ID"},
  {"it", "it-IT"},
  {"lt", "lt-LT"},
  {"lv", "lv-LV"},
  {"nb", "nb-NO"},
  {"nl", "nl-NL"},
  {"pl", "pl-PL"},
  {"pt-BR", "pt-BR"},
  {"pt-PT", "pt-PT"},
  {"ro", "ro-RO"},
  {"ru", "ru-RU"},
  {"sk", "sk-SK"},
  {"sl", "sl-SI"},
  {"sv", "sv-SE"},
  {"tr", "tr-TR"},
  {"vi", "vi-VN"},
};

// This function returns the language-region version of language name.
// e.g. returns hi-IN for hi.
std::string GetSpellCheckLanguageRegion(const std::string& input_language) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(g_supported_spellchecker_languages);
       ++i) {
    if (g_supported_spellchecker_languages[i].language == input_language) {
      return std::string(
          g_supported_spellchecker_languages[i].language_region);
    }
  }

  return input_language;
}

FilePath GetVersionedFileName(const std::string& input_language,
                              const FilePath& dict_dir) {
  // The default dictionary version is 1-2. These versions have been augmented
  // with additional words found by the translation team.
  static const char kDefaultVersionString[] = "-1-2";

  // The following dictionaries have either not been augmented with additional
  // words (version 1-1) or have new words, as well as an upgraded dictionary
  // as of Feb 2009 (version 1-3).
  static const struct {
    // The language input.
    const char* language;

    // The corresponding version.
    const char* version;
  } special_version_string[] = {
    {"en-AU", "-1-1"},
    {"en-GB", "-1-1"},
    {"es-ES", "-1-1"},
    {"nl-NL", "-1-1"},
    {"ru-RU", "-1-1"},
    {"sv-SE", "-1-1"},
    {"he-IL", "-1-1"},
    {"el-GR", "-1-1"},
    {"hi-IN", "-1-1"},
    {"tr-TR", "-1-1"},
    {"et-EE", "-1-1"},
    {"fr-FR", "-1-4"},  // To fix a crash, fr dictionary was updated to 1.4.
    {"lt-LT", "-1-3"},
    {"pl-PL", "-1-3"}
  };

  // Generate the bdict file name using default version string or special
  // version string, depending on the language.
  std::string language = GetSpellCheckLanguageRegion(input_language);
  std::string versioned_bdict_file_name(language + kDefaultVersionString +
                                        ".bdic");
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(special_version_string); ++i) {
    if (language == special_version_string[i].language) {
      versioned_bdict_file_name =
          language + special_version_string[i].version + ".bdic";
      break;
    }
  }

  return dict_dir.AppendASCII(versioned_bdict_file_name);
}

}  // namespace

// Constructed on UI thread.
SpellCheckHost::SpellCheckHost(Observer* observer,
                               const std::string& language,
                               URLRequestContextGetter* request_context_getter)
    : observer_(observer),
      language_(language),
      tried_to_download_(false),
      request_context_getter_(request_context_getter) {
  DCHECK(observer_);
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // TODO(estade): for Windows, we need to fall back to DIR_USER_DATA if
  // DIR_APP_DICTIONARIES is not writeable.
  FilePath dict_dir;
  PathService::Get(chrome::DIR_APP_DICTIONARIES, &dict_dir);
  bdict_file_ = GetVersionedFileName(language, dict_dir);

  FilePath personal_file_directory;
  PathService::Get(chrome::DIR_USER_DATA, &personal_file_directory);
  custom_dictionary_file_ =
      personal_file_directory.Append(chrome::kCustomDictionaryFileName);

  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &SpellCheckHost::Initialize));
}

SpellCheckHost::~SpellCheckHost() {
  if (fd_.fd != -1)
    close(fd_.fd);
}

void SpellCheckHost::UnsetObserver() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  observer_ = NULL;
}

void SpellCheckHost::AddWord(const std::string& word) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  custom_words_.push_back(word);
  ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
          &SpellCheckHost::WriteWordToCustomDictionary, word));
  NotificationService::current()->Notify(
      NotificationType::SPELLCHECK_WORD_ADDED,
      Source<SpellCheckHost>(this), NotificationService::NoDetails());
}

void SpellCheckHost::Initialize() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  if (!observer_)
    return;

  // We set |auto_close| to false because we don't want IPC to close the fd.
  // We will close it manually in the destructor.
  fd_ = base::FileDescriptor(open(bdict_file_.value().c_str(), O_RDONLY),
                             false);

  // File didn't exist. Download it.
  if (fd_.fd == -1 && !tried_to_download_) {
    DownloadDictionary();
    return;
  }

  if (fd_.fd != -1) {
    // Load custom dictionary.
    std::string contents;
    file_util::ReadFileToString(custom_dictionary_file_, &contents);
    std::vector<std::string> list_of_words;
    SplitString(contents, '\n', &list_of_words);
    for (size_t i = 0; i < list_of_words.size(); ++i)
      custom_words_.push_back(list_of_words[i]);
  }

  ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this,
          &SpellCheckHost::InformObserverOfInitialization));
}

void SpellCheckHost::InformObserverOfInitialization() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  if (observer_)
    observer_->SpellCheckHostInitialized();
}

void SpellCheckHost::DownloadDictionary() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  // Determine URL of file to download.
  static const char kDownloadServerUrl[] =
      "http://cache.pack.google.com/edgedl/chrome/dict/";
  GURL url = GURL(std::string(kDownloadServerUrl) + WideToUTF8(
      l10n_util::ToLower(bdict_file_.BaseName().ToWStringHack())));
  fetcher_.reset(new URLFetcher(url, URLFetcher::GET, this));
  fetcher_->set_request_context(request_context_getter_);
  tried_to_download_ = true;
  fetcher_->Start();
}

void SpellCheckHost::WriteWordToCustomDictionary(const std::string& word) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  // Stored in UTF-8.
  std::string word_to_add(word + "\n");
  FILE* f = file_util::OpenFile(custom_dictionary_file_, "a+");
  if (f != NULL)
    fputs(word_to_add.c_str(), f);
  file_util::CloseFile(f);
}

void SpellCheckHost::OnURLFetchComplete(const URLFetcher* source,
                                        const GURL& url,
                                        const URLRequestStatus& status,
                                        int response_code,
                                        const ResponseCookies& cookies,
                                        const std::string& data) {
  DCHECK(source);
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  if ((response_code / 100) != 2) {
    // Initialize will not try to download the file a second time.
    LOG(ERROR) << "Failure to download dictionary.";
    Initialize();
    return;
  }

  // Basic sanity check on the dictionary.
  // There's the small chance that we might see a 200 status code for a body
  // that represents some form of failure.
  if (data.size() < 4 || data[0] != 'B' || data[1] != 'D' || data[2] != 'i' ||
      data[3] != 'c') {
    LOG(ERROR) << "Failure to download dictionary.";
    Initialize();
    return;
  }

  size_t bytes_written =
      file_util::WriteFile(bdict_file_, data.data(), data.length());
  if (bytes_written != data.length()) {
    LOG(ERROR) << "Failure to save dictionary.";
    // To avoid trying to load a partially saved dictionary, shortcut the
    // Initialize() call.
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this,
            &SpellCheckHost::InformObserverOfInitialization));
    return;
  }

  Initialize();
}
