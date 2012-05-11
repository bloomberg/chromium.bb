// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_host_impl.h"

#include <set>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_split.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/browser/spellchecker/spellcheck_platform_mac.h"
#include "chrome/browser/spellchecker/spellcheck_profile_provider.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/hunspell/google/bdict.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

FilePath GetFirstChoiceFilePath(const std::string& language) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath dict_dir;
  PathService::Get(chrome::DIR_APP_DICTIONARIES, &dict_dir);
  return SpellCheckCommon::GetVersionedFileName(language, dict_dir);
}

#if defined(OS_WIN)
FilePath GetFallbackFilePath(const FilePath& first_choice) {
  FilePath dict_dir;
  PathService::Get(chrome::DIR_USER_DATA, &dict_dir);
  return dict_dir.Append(first_choice.BaseName());
}
#endif

void CloseDictionary(base::PlatformFile file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::ClosePlatformFile(file);
}

}  // namespace

// Constructed on UI thread.
SpellCheckHostImpl::SpellCheckHostImpl(
    SpellCheckProfileProvider* profile,
    const std::string& language,
    net::URLRequestContextGetter* request_context_getter,
    SpellCheckHostMetrics* metrics)
    : profile_(profile),
      dictionary_saved_(false),
      language_(language),
      file_(base::kInvalidPlatformFileValue),
      tried_to_download_(false),
      use_platform_spellchecker_(false),
      request_context_getter_(request_context_getter),
      metrics_(metrics),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(profile_);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  FilePath personal_file_directory;
  PathService::Get(chrome::DIR_USER_DATA, &personal_file_directory);
  custom_dictionary_file_ =
      personal_file_directory.Append(chrome::kCustomDictionaryFileName);

  registrar_.Add(weak_ptr_factory_.GetWeakPtr(),
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());
}

SpellCheckHostImpl::~SpellCheckHostImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (file_ != base::kInvalidPlatformFileValue) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            base::Bind(&CloseDictionary, file_));
    file_ = base::kInvalidPlatformFileValue;
  }
}

void SpellCheckHostImpl::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

#if defined(OS_MACOSX)
  if (spellcheck_mac::SpellCheckerAvailable() &&
      spellcheck_mac::PlatformSupportsLanguage(language_)) {
    use_platform_spellchecker_ = true;
    spellcheck_mac::SetLanguage(language_);
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&SpellCheckHostImpl::InformProfileOfInitialization,
                   weak_ptr_factory_.GetWeakPtr()));
    return;
  }
#endif  // OS_MACOSX

  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
      base::Bind(&SpellCheckHostImpl::InitializeDictionaryLocation,
                 base::Unretained(this)),
      base::Bind(&SpellCheckHostImpl::InitializeDictionaryLocationComplete,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SpellCheckHostImpl::UnsetProfile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  profile_ = NULL;
  request_context_getter_ = NULL;
  fetcher_.reset();
  registrar_.RemoveAll();
}

void SpellCheckHostImpl::InitForRenderer(content::RenderProcessHost* process) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Bug 103693: SpellCheckHostImpl and SpellCheckProfile should not
  // depend on Profile interface.
  Profile* profile = Profile::FromBrowserContext(process->GetBrowserContext());
  if (SpellCheckFactory::GetHostForProfile(profile) != this)
    return;

  PrefService* prefs = profile->GetPrefs();
  IPC::PlatformFileForTransit file = IPC::InvalidPlatformFileForTransit();

  if (GetDictionaryFile() != base::kInvalidPlatformFileValue) {
#if defined(OS_POSIX)
    file = base::FileDescriptor(GetDictionaryFile(), false);
#elif defined(OS_WIN)
    ::DuplicateHandle(::GetCurrentProcess(),
                      GetDictionaryFile(),
                      process->GetHandle(),
                      &file,
                      0,
                      false,
                      DUPLICATE_SAME_ACCESS);
#endif
  }

  process->Send(new SpellCheckMsg_Init(
      file,
      profile_ ? profile_->GetCustomWords() : CustomWordList(),
      GetLanguage(),
      prefs->GetBoolean(prefs::kEnableAutoSpellCorrect)));
}

void SpellCheckHostImpl::AddWord(const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (profile_)
    profile_->CustomWordAddedLocally(word);

  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
      base::Bind(&SpellCheckHostImpl::WriteWordToCustomDictionary,
                 base::Unretained(this), word),
      base::Bind(&SpellCheckHostImpl::AddWordComplete,
                 weak_ptr_factory_.GetWeakPtr(), word));
}

void SpellCheckHostImpl::InitializeDictionaryLocation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Initialize the BDICT path. This initialization should be in the FILE thread
  // because it checks if there is a "Dictionaries" directory and create it.
  if (bdict_file_path_.empty())
    bdict_file_path_ = GetFirstChoiceFilePath(language_);

#if defined(OS_WIN)
  // Check if the dictionary exists in the fallback location. If so, use it
  // rather than downloading anew.
  FilePath fallback = GetFallbackFilePath(bdict_file_path_);
  if (!file_util::PathExists(bdict_file_path_) &&
      file_util::PathExists(fallback)) {
    bdict_file_path_ = fallback;
  }
#endif

  if (!profile_)
    return;

  if (!VerifyBDict(bdict_file_path_)) {
    DCHECK_EQ(file_, base::kInvalidPlatformFileValue);
    file_util::Delete(bdict_file_path_, false);

    // Notify browser tests that this dictionary is corrupted. We also skip
    // downloading the dictionary when we run this function on browser tests.
    if (SignalStatusEvent(BDICT_CORRUPTED))
      tried_to_download_ = true;
  } else {
    file_ = base::CreatePlatformFile(
        bdict_file_path_,
        base::PLATFORM_FILE_READ | base::PLATFORM_FILE_OPEN,
        NULL, NULL);
  }

  // File didn't exist. Download it.
  if (file_ == base::kInvalidPlatformFileValue && !tried_to_download_ &&
      request_context_getter_) {
    // Return this function so InitializeDictionaryLocationComplete() can start
    // downloading the dictionary.
    return;
  }

  request_context_getter_ = NULL;

  custom_words_.reset(new CustomWordList());
  if (file_ != base::kInvalidPlatformFileValue)
    LoadCustomDictionary(custom_words_.get());
}

void SpellCheckHostImpl::InitializeDictionaryLocationComplete() {
  if (file_ == base::kInvalidPlatformFileValue && !tried_to_download_ &&
      request_context_getter_) {
    // We download from the ui thread because we need to know that
    // |request_context_getter_| is still valid before initiating the download.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&SpellCheckHostImpl::DownloadDictionary,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(
            &SpellCheckHostImpl::InformProfileOfInitializationWithCustomWords,
            weak_ptr_factory_.GetWeakPtr(),
            custom_words_.release()));
  }
}

void SpellCheckHostImpl::InformProfileOfInitialization() {
  InformProfileOfInitializationWithCustomWords(NULL);
}

void SpellCheckHostImpl::InformProfileOfInitializationWithCustomWords(
    CustomWordList* custom_words) {
  // Non-null |custom_words| should be given only if the profile is available
  // for simplifying the life-cycle management of the word list.
  DCHECK(profile_ || !custom_words);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (profile_)
    profile_->SpellCheckHostInitialized(custom_words);

  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    content::RenderProcessHost* process = i.GetCurrentValue();
    if (process)
      InitForRenderer(process);
  }
}

void SpellCheckHostImpl::DownloadDictionary() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(request_context_getter_);

  // Determine URL of file to download.
  static const char kDownloadServerUrl[] =
      "http://cache.pack.google.com/edgedl/chrome/dict/";
  std::string bdict_file = bdict_file_path_.BaseName().MaybeAsASCII();
  if (bdict_file.empty()) {
    NOTREACHED();
    return;
  }
  GURL url = GURL(std::string(kDownloadServerUrl) +
                  StringToLowerASCII(bdict_file));
  fetcher_.reset(content::URLFetcher::Create(url, content::URLFetcher::GET,
                                weak_ptr_factory_.GetWeakPtr()));
  fetcher_->SetRequestContext(request_context_getter_);
  fetcher_->SetLoadFlags(
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES);
  tried_to_download_ = true;
  fetcher_->Start();
  request_context_getter_ = NULL;
}

void SpellCheckHostImpl::LoadCustomDictionary(CustomWordList* custom_words) {
  if (!custom_words)
    return;

  // Load custom dictionary for profile.
  if (profile_)
    profile_->LoadCustomDictionary(custom_words);

  // Load custom dictionary.
  std::string contents;
  file_util::ReadFileToString(custom_dictionary_file_, &contents);
  if (contents.empty())
    return;

  CustomWordList list_of_words;
  base::SplitString(contents, '\n', &list_of_words);
  for (size_t i = 0; i < list_of_words.size(); ++i) {
    if (list_of_words[i] != "")
      custom_words->push_back(list_of_words[i]);
  }
}

void SpellCheckHostImpl::WriteWordToCustomDictionary(const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (profile_)
    profile_->WriteWordToCustomDictionary(word);
}

void SpellCheckHostImpl::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK(source);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<content::URLFetcher> fetcher_destructor(fetcher_.release());

  if ((source->GetResponseCode() / 100) != 2) {
    // Initialize will not try to download the file a second time.
    LOG(ERROR) << "Failure to download dictionary.";
    return;
  }

  // Basic sanity check on the dictionary.
  // There's the small chance that we might see a 200 status code for a body
  // that represents some form of failure.
  std::string data;
  source->GetResponseAsString(&data);
  if (data.size() < 4 || data[0] != 'B' || data[1] != 'D' || data[2] != 'i' ||
      data[3] != 'c') {
    LOG(ERROR) << "Failure to download dictionary.";
    return;
  }

  data_ = data;
  dictionary_saved_ = false;
  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
      base::Bind(&SpellCheckHostImpl::SaveDictionaryData,
                 base::Unretained(this)),
      base::Bind(&SpellCheckHostImpl::SaveDictionaryDataComplete,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SpellCheckHostImpl::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_RENDERER_PROCESS_CREATED);
  content::RenderProcessHost* process =
      content::Source<content::RenderProcessHost>(source).ptr();
  InitForRenderer(process);
}

void SpellCheckHostImpl::SaveDictionaryData() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // To prevent corrupted dictionary data from causing a renderer crash, scan
  // the dictionary data and verify it is sane before save it to a file.
  bool verified = hunspell::BDict::Verify(data_.data(), data_.size());
  if (metrics_)
    metrics_->RecordDictionaryCorruptionStats(!verified);
  if (!verified) {
    LOG(ERROR) << "Failure to verify the downloaded dictionary.";
    // Let PostTaskAndReply caller send to InformProfileOfInitialization
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
      // Initialize() call. Let PostTaskAndReply caller send to
      // InformProfileOfInitialization through SaveDictionaryDataComplete()
      return;
    }
  }

  data_.clear();
  dictionary_saved_ = true;
}

void SpellCheckHostImpl::SaveDictionaryDataComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (dictionary_saved_) {
    Initialize();
  } else {
    InformProfileOfInitialization();
  }
}

bool SpellCheckHostImpl::VerifyBDict(const FilePath& path) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Read the dictionary file and scan its data. We need to close this file
  // after scanning its data so we can delete it and download a new dictionary
  // from our dictionary-download server if it is corrupted.
  file_util::MemoryMappedFile map;
  if (!map.Initialize(path))
    return false;

  return hunspell::BDict::Verify(
      reinterpret_cast<const char*>(map.data()), map.length());
}

SpellCheckHostMetrics* SpellCheckHostImpl::GetMetrics() const {
  return metrics_;
}

bool SpellCheckHostImpl::IsReady() const {
  return GetDictionaryFile() != base::kInvalidPlatformFileValue ||
      IsUsingPlatformChecker();
}

const base::PlatformFile& SpellCheckHostImpl::GetDictionaryFile() const {
  return file_;
}

const std::string& SpellCheckHostImpl::GetLanguage() const {
  return language_;
}

bool SpellCheckHostImpl::IsUsingPlatformChecker() const {
  return use_platform_spellchecker_;
}

void SpellCheckHostImpl::AddWordComplete(const std::string& word) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->Send(new SpellCheckMsg_WordAdded(word));
  }
}
