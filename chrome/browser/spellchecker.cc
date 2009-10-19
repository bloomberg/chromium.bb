// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/spellchecker_common.h"
#include "chrome/browser/spellchecker_platform_engine.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_counters.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/url_request/url_request.h"
#include "third_party/hunspell/src/hunspell/hunspell.hxx"

using base::TimeTicks;

namespace {

static const struct {
  // The language.
  const char* language;

  // The corresponding language and region, used by the dictionaries.
  const char* language_region;
} g_supported_spellchecker_languages[] = {
  {"en-US", "en-US"},
  {"en-GB", "en-GB"},
  {"en-AU", "en-AU"},
  {"fr", "fr-FR"},
  {"it", "it-IT"},
  {"de", "de-DE"},
  {"es", "es-ES"},
  {"nl", "nl-NL"},
  {"pt-BR", "pt-BR"},
  {"ru", "ru-RU"},
  {"pl", "pl-PL"},
  // {"th", "th-TH"}, // Not to be included in Spellchecker as per B=1277824
  {"sv", "sv-SE"},
  {"da", "da-DK"},
  {"pt-PT", "pt-PT"},
  {"ro", "ro-RO"},
  // {"hu", "hu-HU"}, // Not to be included in Spellchecker as per B=1277824
  {"he", "he-IL"},
  {"id", "id-ID"},
  {"cs", "cs-CZ"},
  {"el", "el-GR"},
  {"nb", "nb-NO"},
  {"vi", "vi-VN"},
  // {"bg", "bg-BG"}, // Not to be included in Spellchecker as per B=1277824
  {"hr", "hr-HR"},
  {"lt", "lt-LT"},
  {"sk", "sk-SK"},
  {"sl", "sl-SI"},
  {"ca", "ca-ES"},
  {"lv", "lv-LV"},
  // {"uk", "uk-UA"}, // Not to be included in Spellchecker as per B=1277824
  {"hi", "hi-IN"},
  {"et", "et-EE"},
  {"tr", "tr-TR"},
};

// Get the fallback folder (currently chrome::DIR_USER_DATA) where the
// dictionary is downloaded in case of system-wide installations.
FilePath GetFallbackDictionaryDownloadDirectory() {
  FilePath dict_dir_userdata;
  PathService::Get(chrome::DIR_USER_DATA, &dict_dir_userdata);
  dict_dir_userdata = dict_dir_userdata.AppendASCII("Dictionaries");
  return dict_dir_userdata;
}

bool SaveBufferToFile(const std::string& data,
                      FilePath file_to_write) {
  int num_bytes = data.length();
  return file_util::WriteFile(file_to_write, data.data(), num_bytes) ==
      num_bytes;
}

}  // namespace

// This is a helper class which acts as a proxy for invoking a task from the
// file loop back to the IO loop. Invoking a task from file loop to the IO
// loop directly is not safe as during browser shutdown, the IO loop tears
// down before the file loop. To avoid a crash, this object is invoked in the
// UI loop from the file loop, from where it gets the IO thread directly from
// g_browser_process and invokes the given task in the IO loop if it is not
// NULL. This object also takes ownership of the given task.
class UIProxyForIOTask : public Task {
 public:
  explicit UIProxyForIOTask(Task* callback_task, SpellChecker* spellchecker)
      : callback_task_(callback_task),
        spellchecker_(spellchecker) {
  }

 private:
  void Run();

  Task* callback_task_;
  // The SpellChecker that invoked the file loop task. May be NULL. If not
  // NULL, then we will Release() on it if we don't run |callback_task_|. This
  // balances any refs the spellchecker might have had outstanding which it
  // would have Released() when |callback_task_| was run.
  SpellChecker* spellchecker_;
  DISALLOW_COPY_AND_ASSIGN(UIProxyForIOTask);
};

void UIProxyForIOTask::Run() {
  // This has been invoked in the UI thread.
  base::Thread* io_thread = g_browser_process->io_thread();
  if (io_thread) {  // io_thread has not been torn down yet.
    MessageLoop* io_loop = io_thread->message_loop();
    io_loop->PostTask(FROM_HERE, callback_task_);
  } else {
    if (spellchecker_)
      spellchecker_->Release();
    delete callback_task_;
  }

  callback_task_ = NULL;
}

// Design: The spellchecker initializes hunspell_ in the Initialize() method.
// This is done using the dictionary file on disk, e.g. "en-US_1_1.bdic".
// Initialization of hunspell_ is held off during this process. If the
// dictionary is not available, we first attempt to download and save it. After
// the dictionary is downloaded and saved to disk (or the attempt to do so
// fails)), corresponding flags are set
// in spellchecker - in the IO thread. Since IO thread goes first during closing
// of browser, a proxy task |UIProxyForIOTask| is created in the UI thread,
// which obtains the IO thread independently and invokes the task in the IO
// thread if it's not NULL. After the flags are cleared, a (final) attempt is
// made to initialize hunspell_. If it fails even then (dictionary could not
// download), no more attempts are made to initialize it.
class SaveDictionaryTask : public Task {
 public:
  SaveDictionaryTask(Task* on_dictionary_save_complete_callback_task,
                     const FilePath& first_attempt_file_name,
                     const FilePath& fallback_file_name,
                     const std::string& data)
      : on_dictionary_save_complete_callback_task_(
            on_dictionary_save_complete_callback_task),
        first_attempt_file_name_(first_attempt_file_name),
        fallback_file_name_(fallback_file_name),
        data_(data) {
  }

 private:
  void Run();

  bool SaveBufferToFile(const std::string& data,
                        FilePath file_to_write) {
    int num_bytes = data.length();
    return file_util::WriteFile(file_to_write, data.data(), num_bytes) ==
        num_bytes;
  }

  // factory object to invokelater back to spellchecker in io thread on
  // download completion to change appropriate flags.
  Task* on_dictionary_save_complete_callback_task_;

  // The file which will be stored in the first attempt.
  FilePath first_attempt_file_name_;

  // The file which will be stored as a fallback.
  FilePath fallback_file_name_;

  // The buffer which has to be stored to disk.
  std::string data_;

  // This invokes back to io loop when downloading is over.
  DISALLOW_COPY_AND_ASSIGN(SaveDictionaryTask);
};

void SaveDictionaryTask::Run() {
  if (!SaveBufferToFile(data_, first_attempt_file_name_)) {
    // Try saving it to |fallback_file_name_|, which almost surely has
    // write permission. If even this fails, there is nothing to be done.
    FilePath fallback_dir = fallback_file_name_.DirName();
    // Create the directory if it does not exist.
    if (!file_util::PathExists(fallback_dir))
      file_util::CreateDirectory(fallback_dir);
    SaveBufferToFile(data_, fallback_file_name_);
  }  // Unsuccessful save is taken care of in SpellChecker::Initialize().

  // Set Flag that dictionary is not downloading anymore.
  MessageLoop* ui_loop = ChromeThread::GetMessageLoop(ChromeThread::UI);
  ui_loop->PostTask(FROM_HERE,
      new UIProxyForIOTask(on_dictionary_save_complete_callback_task_, NULL));
}

// Design: this task tries to read the dictionary from disk and load it into
// memory. It is executed on the file thread, and posts the results back to
// the IO thread (via the UI thread---see UIProxyForIOTask).
// The task first checks for the existence of the dictionary in one of the two
// given locations. If it does not exist, the task informs the SpellChecker,
// which will try to download the directory and run a new ReadDictionaryTask.
class ReadDictionaryTask : public Task {
 public:
  ReadDictionaryTask(SpellChecker* spellchecker,
                     const FilePath& dict_file_name_app,
                     const FilePath& dict_file_name_usr)
        : spellchecker_(spellchecker),
          hunspell_(NULL),
          bdict_file_(NULL),
          custom_dictionary_file_name_(
              spellchecker->custom_dictionary_file_name_),
          dict_file_name_app_(dict_file_name_app),
          dict_file_name_usr_(dict_file_name_usr) {
  }

  virtual void Run() {
    FilePath bdict_file_path;
    if (file_util::PathExists(dict_file_name_app_)) {
      bdict_file_path = dict_file_name_app_;
    } else if (file_util::PathExists(dict_file_name_usr_)) {
      bdict_file_path = dict_file_name_usr_;
    } else {
      Finish(false);
      return;
    }

    bdict_file_ = new file_util::MemoryMappedFile;
    if (bdict_file_->Initialize(bdict_file_path)) {
      TimeTicks start_time = TimeTicks::Now();

      hunspell_ =
          new Hunspell(bdict_file_->data(), bdict_file_->length());

      // Add custom words to Hunspell.
      std::string contents;
      file_util::ReadFileToString(custom_dictionary_file_name_, &contents);
      std::vector<std::string> list_of_words;
      SplitString(contents, '\n', &list_of_words);
      for (std::vector<std::string>::iterator it = list_of_words.begin();
           it != list_of_words.end(); ++it) {
        hunspell_->add(it->c_str());
      }

      DHISTOGRAM_TIMES("Spellcheck.InitTime",
                       TimeTicks::Now() - start_time);
    } else {
      delete bdict_file_;
      bdict_file_ = NULL;
    }

    Finish(true);
  }

 private:
  void Finish(bool file_existed) {
    Task* task = NewRunnableMethod(spellchecker_, &SpellChecker::HunspellInited,
                                   hunspell_, bdict_file_, file_existed);
    if (spellchecker_->file_loop_) {
      MessageLoop* ui_loop = ChromeThread::GetMessageLoop(ChromeThread::UI);
      // We were called on the file loop. Post back to the IO loop.
      // If this never gets posted to the IO loop, then we will leak |hunspell_|
      // and |bdict_file_|. But that can only happen during shutdown, so it's
      // not worth caring about.
      ui_loop->PostTask(FROM_HERE, new UIProxyForIOTask(task, spellchecker_));
    } else {
      // We were called directly (e.g., during testing). Run the task directly.
      task->Run();
      delete task;
    }
  }

  // The SpellChecker we are working for. We are guaranteed to be outlived
  // by this object because it AddRefs() itself before calling us.
  // Accessing it is not necessarily thread safe, but are careful to only access
  // it in ways that are.
  SpellChecker* spellchecker_;
  Hunspell* hunspell_;
  file_util::MemoryMappedFile* bdict_file_;

  FilePath custom_dictionary_file_name_;
  FilePath dict_file_name_app_;
  FilePath dict_file_name_usr_;

  DISALLOW_COPY_AND_ASSIGN(ReadDictionaryTask);
};

void SpellChecker::SpellCheckLanguages(std::vector<std::string>* languages) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(g_supported_spellchecker_languages);
       ++i) {
    languages->push_back(g_supported_spellchecker_languages[i].language);
  }
}

// This function returns the language-region version of language name.
// e.g. returns hi-IN for hi.
std::string SpellChecker::GetSpellCheckLanguageRegion(
    std::string input_language) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(g_supported_spellchecker_languages);
       ++i) {
    std::string language(
        g_supported_spellchecker_languages[i].language);
    if (language ==  input_language)
      return std::string(
          g_supported_spellchecker_languages[i].language_region);
  }

  return input_language;
}


std::string SpellChecker::GetLanguageFromLanguageRegion(
    std::string input_language) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(g_supported_spellchecker_languages);
       ++i) {
    std::string language(
        g_supported_spellchecker_languages[i].language_region);
    if (language ==  input_language)
      return std::string(g_supported_spellchecker_languages[i].language);
  }

  return input_language;
}

std::string SpellChecker::GetCorrespondingSpellCheckLanguage(
    const std::string& language) {
  // Look for exact match in the Spell Check language list.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(g_supported_spellchecker_languages);
       ++i) {
    // First look for exact match in the language region of the list.
    std::string spellcheck_language(
        g_supported_spellchecker_languages[i].language);
    if (spellcheck_language == language)
      return language;

    // Next, look for exact match in the language_region part of the list.
    std::string spellcheck_language_region(
        g_supported_spellchecker_languages[i].language_region);
    if (spellcheck_language_region == language)
      return g_supported_spellchecker_languages[i].language;
  }

  // Look for a match by comparing only language parts. All the 'en-RR'
  // except for 'en-GB' exactly matched in the above loop, will match
  // 'en-US'. This is not ideal because 'en-ZA', 'en-NZ' had
  // better be matched with 'en-GB'. This does not handle cases like
  // 'az-Latn-AZ' vs 'az-Arab-AZ', either, but we don't use 3-part
  // locale ids with a script code in the middle, yet.
  // TODO(jungshik): Add a better fallback.
  std::string language_part(language, 0, language.find('-'));
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(g_supported_spellchecker_languages);
       ++i) {
    std::string spellcheck_language(
        g_supported_spellchecker_languages[i].language_region);
    if (spellcheck_language.substr(0, spellcheck_language.find('-')) ==
        language_part)
      return spellcheck_language;
  }

  // No match found - return blank.
  return std::string();
}

int SpellChecker::GetSpellCheckLanguages(
    Profile* profile,
    std::vector<std::string>* languages) {
  StringPrefMember accept_languages_pref;
  StringPrefMember dictionary_language_pref;
  accept_languages_pref.Init(prefs::kAcceptLanguages, profile->GetPrefs(),
                             NULL);
  dictionary_language_pref.Init(prefs::kSpellCheckDictionary,
                                profile->GetPrefs(), NULL);
  std::string dictionary_language =
      WideToASCII(dictionary_language_pref.GetValue());

  // The current dictionary language should be there.
  languages->push_back(dictionary_language);

  // Now scan through the list of accept languages, and find possible mappings
  // from this list to the existing list of spell check languages.
  std::vector<std::string> accept_languages;

  if (SpellCheckerPlatform::SpellCheckerAvailable()) {
    SpellCheckerPlatform::GetAvailableLanguages(&accept_languages);
  } else {
    SplitString(WideToASCII(accept_languages_pref.GetValue()), ',',
                &accept_languages);
  }
  for (std::vector<std::string>::const_iterator i = accept_languages.begin();
       i != accept_languages.end(); ++i) {
    std::string language = GetCorrespondingSpellCheckLanguage(*i);
    if (!language.empty() &&
        std::find(languages->begin(), languages->end(), language) ==
        languages->end())
      languages->push_back(language);
  }

  for (size_t i = 0; i < languages->size(); ++i) {
    if ((*languages)[i] == dictionary_language)
      return i;
  }
  return -1;
}

FilePath SpellChecker::GetVersionedFileName(const std::string& input_language,
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
    {"fr-FR", "-1-4"}, // to fix crash, fr dictionary was updated to 1.4
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

SpellChecker::SpellChecker(const FilePath& dict_dir,
                           const std::string& language,
                           URLRequestContext* request_context,
                           const FilePath& custom_dictionary_file_name)
    : given_dictionary_directory_(dict_dir),
      custom_dictionary_file_name_(custom_dictionary_file_name),
      tried_to_init_(false),
      language_(language),
      worker_loop_(NULL),
      tried_to_download_dictionary_file_(false),
      file_loop_(NULL),
      url_request_context_(request_context),
      obtaining_dictionary_(false),
      auto_spell_correct_turned_on_(false),
      is_using_platform_spelling_engine_(false),
      fetcher_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  if (SpellCheckerPlatform::SpellCheckerAvailable()) {
    SpellCheckerPlatform::Init();
    if (SpellCheckerPlatform::PlatformSupportsLanguage(language)) {
      // If we have reached here, then we know that the current platform
      // supports the given language and we will use it instead of hunspell.
      SpellCheckerPlatform::SetLanguage(language);
      is_using_platform_spelling_engine_ = true;
    }
  }

  // Get the corresponding BDIC file name.
  bdic_file_name_ = GetVersionedFileName(language, dict_dir).BaseName();

  // Get File Loop - hunspell gets initialized here.
  base::Thread* file_thread = g_browser_process->file_thread();
  if (file_thread)
    file_loop_ = file_thread->message_loop();

  // Get the path to the custom dictionary file.
  if (custom_dictionary_file_name_.empty()) {
    FilePath personal_file_directory;
    PathService::Get(chrome::DIR_USER_DATA, &personal_file_directory);
    custom_dictionary_file_name_ =
        personal_file_directory.Append(chrome::kCustomDictionaryFileName);
  }

  // Use this dictionary language as the default one of the
  // SpellcheckCharAttribute object.
  character_attributes_.SetDefaultLanguage(language);
}

SpellChecker::~SpellChecker() {
  // This must be deleted on the I/O thread (see the header). This is the same
  // thread that SpellCheckWord is called on, so we verify that they were all
  // the same thread.
  if (worker_loop_)
    DCHECK(MessageLoop::current() == worker_loop_);
}

void SpellChecker::StartDictionaryDownload(const FilePath& file_name) {
  // Determine URL of file to download.
  static const char kDownloadServerUrl[] =
      "http://cache.pack.google.com/edgedl/chrome/dict/";
  GURL url = GURL(std::string(kDownloadServerUrl) + WideToUTF8(
      l10n_util::ToLower(bdic_file_name_.ToWStringHack())));
  fetcher_.reset(new URLFetcher(url, URLFetcher::GET, this));
  fetcher_->set_request_context(url_request_context_);
  obtaining_dictionary_ = true;
  fetcher_->Start();
}

void SpellChecker::OnURLFetchComplete(const URLFetcher* source,
                                      const GURL& url,
                                      const URLRequestStatus& status,
                                      int response_code,
                                      const ResponseCookies& cookies,
                                      const std::string& data) {
  DCHECK(source);
  if ((response_code / 100) != 2) {
    obtaining_dictionary_ = false;
    return;
  }

  // Basic sanity check on the dictionary.
  // There's the small chance that we might see a 200 status code for a body
  // that represents some form of failure.
  if (data.size() < 4 || data[0] != 'B' || data[1] != 'D' || data[2] != 'i' ||
      data[3] != 'c') {
    obtaining_dictionary_ = false;
    return;
  }

  // Save the file in the file thread, and not here, the IO thread.
  FilePath first_attempt_file_name = given_dictionary_directory_.Append(
      bdic_file_name_);
  FilePath user_data_dir = GetFallbackDictionaryDownloadDirectory();
  FilePath fallback_file_name = user_data_dir.Append(bdic_file_name_);
  Task* dic_task = method_factory_.
      NewRunnableMethod(&SpellChecker::OnDictionarySaveComplete);
  file_loop_->PostTask(FROM_HERE, new SaveDictionaryTask(dic_task,
      first_attempt_file_name, fallback_file_name, data));
}

void SpellChecker::OnDictionarySaveComplete() {
  obtaining_dictionary_ = false;
  // Now that the dictionary is downloaded, continue trying to download.
  Initialize();
}

// Initialize SpellChecker. In this method, if the dictionary is not present
// in the local disk, it is fetched asynchronously.
bool SpellChecker::Initialize() {
  if (!worker_loop_)
    worker_loop_ = MessageLoop::current();
  else
    DCHECK(worker_loop_ == MessageLoop::current());

  // Return false if the dictionary files are downloading.
  if (obtaining_dictionary_)
    return false;

  // Return false if tried to init and failed - don't try multiple times in
  // this session.
  if (tried_to_init_)
    return hunspell_.get() != NULL;

  StatsScope<StatsCounterTimer> timer(chrome::Counters::spellcheck_init());

  // The default place whether the spellcheck dictionary can reside is
  // chrome::DIR_APP_DICTIONARIES. However, for systemwide installations,
  // this directory may not have permissions for download. In that case, the
  // alternate directory for download is chrome::DIR_USER_DATA. We have to check
  // for the spellcheck dictionaries in both the directories. If not found in
  // either one, it has to be downloaded in either of the two.
  // TODO(sidchat): Some sort of UI to warn users that spellchecker is not
  // working at all (due to failed dictionary download)?

  // File name for downloading in DIR_APP_DICTIONARIES.
  FilePath dictionary_file_name_app = GetVersionedFileName(language_,
      given_dictionary_directory_);

  // Filename for downloading in the fallback dictionary download directory,
  // DIR_USER_DATA.
  FilePath dict_dir_userdata = GetFallbackDictionaryDownloadDirectory();
  FilePath dictionary_file_name_usr = GetVersionedFileName(language_,
      dict_dir_userdata);

  // Balances Release() in HunspellInited(), or in UIProxyForIOTask if the IO
  // thread is torn down before the ReadDictionaryTask calls us back.
  AddRef();
  Task* task = new ReadDictionaryTask(this,
      dictionary_file_name_app, dictionary_file_name_usr);
  if (file_loop_) {
    file_loop_->PostTask(FROM_HERE, task);
  } else {
    task->Run();
    delete task;
  }

  return hunspell_.get() != NULL;
}

void SpellChecker::HunspellInited(Hunspell* hunspell,
                                  file_util::MemoryMappedFile* bdict_file,
                                  bool file_existed) {
  DCHECK(worker_loop_ == MessageLoop::current());

  if (file_existed)
    tried_to_init_ = true;

  if (!hunspell) {
    if (!file_existed) {
      // File didn't exist. We need to download a dictionary.
      DoDictionaryDownload();
    }

    // Balances AddRef() in Initialize().
    Release();
    return;
  }


  bdict_file_.reset(bdict_file);
  hunspell_.reset(hunspell);
  // Add all the custom words we've gotten while Hunspell was loading.
  while (!custom_words_.empty()) {
    hunspell_->add(custom_words_.front().c_str());
    custom_words_.pop();
  }

  // Balances AddRef() in Initialize().
  Release();
}

void SpellChecker::DoDictionaryDownload() {
  // Download the dictionary file.
  if (file_loop_ && url_request_context_) {
    if (!tried_to_download_dictionary_file_) {
      FilePath dictionary_file_name_app = GetVersionedFileName(language_,
          given_dictionary_directory_);
      StartDictionaryDownload(dictionary_file_name_app);
      tried_to_download_dictionary_file_ = true;
    } else {
      // Don't try to download a dictionary more than once.
      tried_to_init_ = true;
    }
  } else {
    NOTREACHED();
  }
}

string16 SpellChecker::GetAutoCorrectionWord(const string16& word, int tag) {
  string16 autocorrect_word;
  if (!auto_spell_correct_turned_on_)
    return autocorrect_word;  // Return the empty string.

  int word_length = static_cast<int>(word.size());
  if (word_length < 2 || word_length > kMaxAutoCorrectWordSize)
    return autocorrect_word;

  char16 misspelled_word[kMaxAutoCorrectWordSize + 1];
  const char16* word_char = word.c_str();
  for (int i = 0; i <= kMaxAutoCorrectWordSize; i++) {
    if (i >= word_length)
      misspelled_word[i] = NULL;
    else
      misspelled_word[i] = word_char[i];
  }

  // Swap adjacent characters and spellcheck.
  int misspelling_start, misspelling_len;
  for (int i = 0; i < word_length - 1; i++) {
    // Swap.
    std::swap(misspelled_word[i], misspelled_word[i + 1]);

    // Check spelling.
    misspelling_start = misspelling_len = 0;
    SpellCheckWord(misspelled_word, word_length, tag, &misspelling_start,
        &misspelling_len, NULL);

    // Make decision: if only one swap produced a valid word, then we want to
    // return it. If we found two or more, we don't do autocorrection.
    if (misspelling_len == 0) {
      if (autocorrect_word.empty()) {
        autocorrect_word.assign(misspelled_word);
      } else {
        autocorrect_word.clear();
        break;
      }
    }

    // Restore the swapped characters.
    std::swap(misspelled_word[i], misspelled_word[i + 1]);
  }
  return autocorrect_word;
}

void SpellChecker::EnableAutoSpellCorrect(bool turn_on) {
  auto_spell_correct_turned_on_ = turn_on;
}

// Returns whether or not the given string is a valid contraction.
// This function is a fall-back when the SpellcheckWordIterator class
// returns a concatenated word which is not in the selected dictionary
// (e.g. "in'n'out") but each word is valid.
bool SpellChecker::IsValidContraction(const string16& contraction, int tag) {
  SpellcheckWordIterator word_iterator;
  word_iterator.Initialize(&character_attributes_, contraction.c_str(),
                           contraction.length(), false);

  string16 word;
  int word_start;
  int word_length;
  while (word_iterator.GetNextWord(&word, &word_start, &word_length)) {
    if (!CheckSpelling(word, tag))
      return false;
  }
  return true;
}

bool SpellChecker::SpellCheckWord(
    const char16* in_word,
    int in_word_len,
    int tag,
    int* misspelling_start,
    int* misspelling_len,
    std::vector<string16>* optional_suggestions) {
  DCHECK(in_word_len >= 0);
  DCHECK(misspelling_start && misspelling_len) << "Out vars must be given.";

  // This must always be called on the same thread (normally the I/O thread).
  if (worker_loop_)
    DCHECK(MessageLoop::current() == worker_loop_);

  // Check if the platform spellchecker is being used.
  if (!is_using_platform_spelling_engine_) {
    // If it isn't, try and init hunspell.
    Initialize();

    // Check to see if hunspell was successfuly initialized.
    if (!hunspell_.get())
      return true;  // Unable to spellcheck, return word is OK.
  }

  StatsScope<StatsRate> timer(chrome::Counters::spellcheck_lookup());

  *misspelling_start = 0;
  *misspelling_len = 0;
  if (in_word_len == 0)
    return true;  // No input means always spelled correctly.

  SpellcheckWordIterator word_iterator;
  string16 word;
  int word_start;
  int word_length;
  word_iterator.Initialize(&character_attributes_, in_word, in_word_len, true);
  while (word_iterator.GetNextWord(&word, &word_start, &word_length)) {
    // Found a word (or a contraction) that the spellchecker can check the
    // spelling of.
    bool word_ok = CheckSpelling(word, tag);
    if (word_ok)
      continue;

    // If the given word is a concatenated word of two or more valid words
    // (e.g. "hello:hello"), we should treat it as a valid word.
    if (IsValidContraction(word, tag))
      continue;

    *misspelling_start = word_start;
    *misspelling_len = word_length;

    // Get the list of suggested words.
    if (optional_suggestions)
      FillSuggestionList(word, optional_suggestions);
    return false;
  }

  return true;
}

// This task is called in the file loop to write the new word to the custom
// dictionary in disc.
class AddWordToCustomDictionaryTask : public Task {
 public:
  AddWordToCustomDictionaryTask(const FilePath& file_name,
                                const string16& word)
      : file_name_(file_name),
        word_(UTF16ToUTF8(word)) {
  }

 private:
  void Run();

  FilePath file_name_;
  std::string word_;
};

void AddWordToCustomDictionaryTask::Run() {
  // Add the word with a new line. Note that, although this would mean an
  // extra line after the list of words, this is potentially harmless and
  // faster, compared to verifying everytime whether to append a new line
  // or not.
  word_ += "\n";
  FILE* f = file_util::OpenFile(file_name_, "a+");
  if (f != NULL)
    fputs(word_.c_str(), f);
  file_util::CloseFile(f);
}

void SpellChecker::AddWord(const string16& word) {
  if (is_using_platform_spelling_engine_) {
    SpellCheckerPlatform::AddWord(word);
    return;
  }

  // Check if the |hunspell_| has been initialized at all.
  Initialize();

  // Add the word to hunspell.
  std::string word_to_add = UTF16ToUTF8(word);
  if (!word_to_add.empty()) {
    // Either add the word to |hunspell_|, or, if |hunspell_| is still loading,
    // defer it till after the load completes.
    if (hunspell_.get())
      hunspell_->add(word_to_add.c_str());
    else
      custom_words_.push(word_to_add);
  }

  // Now add the word to the custom dictionary file.
  Task* write_word_task =
      new AddWordToCustomDictionaryTask(custom_dictionary_file_name_, word);
  if (file_loop_) {
    file_loop_->PostTask(FROM_HERE, write_word_task);
  } else {
    write_word_task->Run();
    delete write_word_task;
  }
}

bool SpellChecker::CheckSpelling(const string16& word_to_check, int tag) {
  bool word_correct = false;

  TimeTicks begin_time = TimeTicks::Now();
  if (is_using_platform_spelling_engine_) {
    word_correct = SpellCheckerPlatform::CheckSpelling(word_to_check, tag);
  } else {
    // |hunspell_->spell| returns 0 if the word is spelled correctly and
    // non-zero otherwsie.
    word_correct = (hunspell_->spell(UTF16ToUTF8(word_to_check).c_str()) != 0);
  }
  DHISTOGRAM_TIMES("Spellcheck.CheckTime", TimeTicks::Now() - begin_time);

  return word_correct;
}

void SpellChecker::FillSuggestionList(
    const string16& wrong_word,
    std::vector<string16>* optional_suggestions) {
  if (is_using_platform_spelling_engine_) {
    SpellCheckerPlatform::FillSuggestionList(wrong_word, optional_suggestions);
    return;
  }
  char** suggestions;
  TimeTicks begin_time = TimeTicks::Now();
  int number_of_suggestions = hunspell_->suggest(&suggestions,
      UTF16ToUTF8(wrong_word).c_str());
  DHISTOGRAM_TIMES("Spellcheck.SuggestTime",
                   TimeTicks::Now() - begin_time);

  // Populate the vector of WideStrings.
  for (int i = 0; i < number_of_suggestions; i++) {
    if (i < kMaxSuggestions)
      optional_suggestions->push_back(UTF8ToUTF16(suggestions[i]));
    free(suggestions[i]);
  }
  if (suggestions != NULL)
    free(suggestions);
}
