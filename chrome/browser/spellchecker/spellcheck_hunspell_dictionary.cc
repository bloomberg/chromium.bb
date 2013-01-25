// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_hunspell_dictionary.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/spellchecker/spellcheck_platform_mac.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "googleurl/src/gurl.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/hunspell/google/bdict.h"

using content::BrowserThread;

namespace {

void CloseDictionary(base::PlatformFile file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::ClosePlatformFile(file);
}

}  // namespace

SpellcheckHunspellDictionary::SpellcheckHunspellDictionary(
    Profile* profile,
    const std::string& language,
    net::URLRequestContextGetter* request_context_getter,
    SpellcheckService* spellcheck_service)
    : SpellcheckDictionary(profile),
      dictionary_saved_(false),
      language_(language),
      file_(base::kInvalidPlatformFileValue),
      tried_to_download_(false),
      use_platform_spellchecker_(false),
      request_context_getter_(request_context_getter),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      spellcheck_service_(spellcheck_service),
      download_status_(DOWNLOAD_NONE) {
}

SpellcheckHunspellDictionary::~SpellcheckHunspellDictionary() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (file_ != base::kInvalidPlatformFileValue) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            base::Bind(&CloseDictionary, file_));
    file_ = base::kInvalidPlatformFileValue;
  }
}

void SpellcheckHunspellDictionary::Load() {
  Initialize();
}

void SpellcheckHunspellDictionary::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

#if defined(OS_MACOSX)
  if (spellcheck_mac::SpellCheckerAvailable() &&
      spellcheck_mac::PlatformSupportsLanguage(language_)) {
    use_platform_spellchecker_ = true;
    spellcheck_mac::SetLanguage(language_);
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(
            &SpellcheckHunspellDictionary::InformListenersOfInitialization,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }
#endif  // OS_MACOSX

  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
      base::Bind(&SpellcheckHunspellDictionary::InitializeDictionaryLocation,
                 base::Unretained(this)),
      base::Bind(
          &SpellcheckHunspellDictionary::InitializeDictionaryLocationComplete,
          weak_ptr_factory_.GetWeakPtr()));
}

void SpellcheckHunspellDictionary::InitializeDictionaryLocation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Initialize the BDICT path. Initialization should be in the FILE thread
  // because it checks if there is a "Dictionaries" directory and create it.
  if (bdict_file_path_.empty()) {
    FilePath dict_dir;
    PathService::Get(chrome::DIR_APP_DICTIONARIES, &dict_dir);
    bdict_file_path_ =
        chrome::spellcheck_common::GetVersionedFileName(language_, dict_dir);
  }

#if defined(OS_WIN)
  // Check if the dictionary exists in the fallback location. If so, use it
  // rather than downloading anew.
  FilePath user_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_dir);
  FilePath fallback = user_dir.Append(bdict_file_path_.BaseName());
  if (!file_util::PathExists(bdict_file_path_) &&
      file_util::PathExists(fallback)) {
    bdict_file_path_ = fallback;
  }
#endif

  if (!VerifyBDict(bdict_file_path_)) {
    DCHECK_EQ(file_, base::kInvalidPlatformFileValue);
    file_util::Delete(bdict_file_path_, false);

    // Notify browser tests that this dictionary is corrupted. We also skip
    // downloading the dictionary when we run this function on browser tests.
    if (spellcheck_service_->SignalStatusEvent(
        SpellcheckService::BDICT_CORRUPTED))
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
}


void SpellcheckHunspellDictionary::InitializeDictionaryLocationComplete() {
  if (file_ == base::kInvalidPlatformFileValue && !tried_to_download_ &&
      request_context_getter_) {
    // We download from the ui thread because we need to know that
    // |request_context_getter_| is still valid before initiating the download.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&SpellcheckHunspellDictionary::DownloadDictionary,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(
            &SpellcheckHunspellDictionary::InformListenersOfInitialization,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void SpellcheckHunspellDictionary::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(source);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<net::URLFetcher> fetcher_destructor(fetcher_.release());

  if ((source->GetResponseCode() / 100) != 2) {
    // Initialize will not try to download the file a second time.
    LOG(ERROR) << "Failure to download dictionary.";
    InformListenersOfDownloadFailure();
    return;
  }

  // Basic sanity check on the dictionary.
  // There's the small chance that we might see a 200 status code for a body
  // that represents some form of failure.
  std::string* data = new std::string();
  source->GetResponseAsString(data);
  if (data->size() < 4 || (*data)[0] != 'B' || (*data)[1] != 'D' ||
      (*data)[2] != 'i' || (*data)[3] != 'c') {
    LOG(ERROR) << "Download of dictionary did not pass check.";
    InformListenersOfDownloadFailure();
    return;
  }

  dictionary_saved_ = false;
  BrowserThread::PostTaskAndReply(BrowserThread::FILE, FROM_HERE,
      base::Bind(&SpellcheckHunspellDictionary::SaveDictionaryData,
                 base::Unretained(this), base::Owned(data)),
      base::Bind(&SpellcheckHunspellDictionary::SaveDictionaryDataComplete,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SpellcheckHunspellDictionary::DownloadDictionary() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(request_context_getter_);

  download_status_ = DOWNLOAD_IN_PROGRESS;
  FOR_EACH_OBSERVER(Observer, observers_, OnHunspellDictionaryDownloadBegin());

  // Determine URL of file to download.
  static const char kDownloadServerUrl[] =
      "http://cache.pack.google.com/edgedl/chrome/dict/";
  std::string bdict_file = bdict_file_path_.BaseName().MaybeAsASCII();
  if (bdict_file.empty()) {
    InformListenersOfDownloadFailure();
    NOTREACHED();
    return;
  }
  GURL url = GURL(std::string(kDownloadServerUrl) +
                  StringToLowerASCII(bdict_file));
  fetcher_.reset(net::URLFetcher::Create(url, net::URLFetcher::GET,
                                         weak_ptr_factory_.GetWeakPtr()));
  fetcher_->SetRequestContext(request_context_getter_);
  fetcher_->SetLoadFlags(
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES);
  tried_to_download_ = true;
  fetcher_->Start();
  request_context_getter_ = NULL;
}

void SpellcheckHunspellDictionary::RetryDownloadDictionary(
      net::URLRequestContextGetter* request_context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  request_context_getter_ = request_context_getter;
  DownloadDictionary();
}

void SpellcheckHunspellDictionary::SaveDictionaryData(std::string* data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // To prevent corrupted dictionary data from causing a renderer crash, scan
  // the dictionary data and verify it is sane before save it to a file.
  bool verified = hunspell::BDict::Verify(data->data(), data->size());
  // TODO(rlp): Adding metrics to RecordDictionaryCorruptionStats
  if (!verified) {
    LOG(ERROR) << "Failure to verify the downloaded dictionary.";
    // Let PostTaskAndReply caller send to InformListenersOfInitialization
    // through SaveDictionaryDataComplete().
    return;
  }

  size_t bytes_written =
      file_util::WriteFile(bdict_file_path_, data->data(), data->length());
  if (bytes_written != data->length()) {
    bool success = false;
#if defined(OS_WIN)
    FilePath dict_dir;
    PathService::Get(chrome::DIR_USER_DATA, &dict_dir);
    FilePath fallback_file_path = dict_dir.Append(bdict_file_path_.BaseName());
    bytes_written =
        file_util::WriteFile(fallback_file_path, data->data(), data->length());
    if (bytes_written == data->length())
      success = true;
#endif
    data->clear();

    if (!success) {
      LOG(ERROR) << "Failure to save dictionary.";
      file_util::Delete(bdict_file_path_, false);
      // To avoid trying to load a partially saved dictionary, shortcut the
      // Initialize() call. Let PostTaskAndReply caller send to
      // InformListenersOfInitialization through SaveDictionaryDataComplete().
      return;
    }
  }

  data->clear();
  dictionary_saved_ = true;
}

void SpellcheckHunspellDictionary::SaveDictionaryDataComplete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (dictionary_saved_) {
    download_status_ = DOWNLOAD_NONE;
    FOR_EACH_OBSERVER(Observer,
                      observers_,
                      OnHunspellDictionaryDownloadSuccess());
    Initialize();
  } else {
    InformListenersOfDownloadFailure();
    InformListenersOfInitialization();
  }
}

bool SpellcheckHunspellDictionary::VerifyBDict(const FilePath& path) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Read the dictionary file and scan its data. We need to close this file
  // after scanning its data so we can delete it and download a new dictionary
  // from our dictionary-download server if it is corrupted.
  file_util::MemoryMappedFile map;
  if (!file_util::PathExists(path) || !map.Initialize(path))
    return false;

  return hunspell::BDict::Verify(
      reinterpret_cast<const char*>(map.data()), map.length());
}

bool SpellcheckHunspellDictionary::IsReady() const {
  return GetDictionaryFile() !=
      base::kInvalidPlatformFileValue || IsUsingPlatformChecker();
}

const base::PlatformFile&
SpellcheckHunspellDictionary::GetDictionaryFile() const {
  return file_;
}

const std::string& SpellcheckHunspellDictionary::GetLanguage() const {
  return language_;
}

bool SpellcheckHunspellDictionary::IsUsingPlatformChecker() const {
  return use_platform_spellchecker_;
}

void SpellcheckHunspellDictionary::AddObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  observers_.AddObserver(observer);
}

void SpellcheckHunspellDictionary::RemoveObserver(Observer* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  observers_.RemoveObserver(observer);
}

bool SpellcheckHunspellDictionary::IsDownloadInProgress() {
  return download_status_ == DOWNLOAD_IN_PROGRESS;
}

bool SpellcheckHunspellDictionary::IsDownloadFailure() {
  return download_status_ == DOWNLOAD_FAILED;
}

void SpellcheckHunspellDictionary::InformListenersOfInitialization() {
  FOR_EACH_OBSERVER(Observer, observers_, OnHunspellDictionaryInitialized());
}

void SpellcheckHunspellDictionary::InformListenersOfDownloadFailure() {
  download_status_ = DOWNLOAD_FAILED;
  FOR_EACH_OBSERVER(Observer,
                    observers_,
                    OnHunspellDictionaryDownloadFailure());
}
