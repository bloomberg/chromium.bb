// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_hunspell_dictionary.h"

#include "base/file_util.h"
#include "base/files/memory_mapped_file.h"
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

// Close the platform file.
void CloseDictionary(base::PlatformFile file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  base::ClosePlatformFile(file);
}

}  // namespace

// Dictionary file information to be passed between the FILE and UI threads.
struct DictionaryFile {
  DictionaryFile() : descriptor(base::kInvalidPlatformFileValue) {
  }

  ~DictionaryFile() {
    if (descriptor != base::kInvalidPlatformFileValue) {
      BrowserThread::PostTask(
          BrowserThread::FILE,
          FROM_HERE,
          base::Bind(&CloseDictionary, descriptor));
      descriptor = base::kInvalidPlatformFileValue;
    }
  }

  // The desired location of the dictionary file, whether or not it exists.
  base::FilePath path;

  // The file descriptor/handle for the dictionary file.
  base::PlatformFile descriptor;

  DISALLOW_COPY_AND_ASSIGN(DictionaryFile);
};

namespace {

// Figures out the location for the dictionary, verifies its contents, and opens
// it. The default place where the spellcheck dictionary resides is
// chrome::DIR_APP_DICTIONARIES. For systemwide installations on Windows.
// however, this directory may not have permissions for download. In that case,
// the alternate directory for download is chrome::DIR_USER_DATA. Returns a
// scoped pointer to avoid leaking the file descriptor if the caller has been
// destroyed.
scoped_ptr<DictionaryFile> InitializeDictionaryLocation(
    const std::string& language) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<DictionaryFile> file(new DictionaryFile);

  // Initialize the BDICT path. Initialization should be in the FILE thread
  // because it checks if there is a "Dictionaries" directory and create it.
  base::FilePath dict_dir;
  PathService::Get(chrome::DIR_APP_DICTIONARIES, &dict_dir);
  file->path = chrome::spellcheck_common::GetVersionedFileName(
      language, dict_dir);

#if defined(OS_WIN)
  // Check if the dictionary exists in the fallback location. If so, use it
  // rather than downloading anew.
  base::FilePath user_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_dir);
  base::FilePath fallback = user_dir.Append(file->path.BaseName());
  if (!file_util::PathExists(file->path) && file_util::PathExists(fallback))
    file->path = fallback;
#endif

  // Read the dictionary file and scan its data to check for corruption. The
  // scoping closes the memory-mapped file before it is opened or deleted.
  bool bdict_is_valid;
  {
    base::MemoryMappedFile map;
    bdict_is_valid = file_util::PathExists(file->path) &&
        map.Initialize(file->path) &&
        hunspell::BDict::Verify(reinterpret_cast<const char*>(map.data()),
                                map.length());
  }
  if (bdict_is_valid) {
    file->descriptor = base::CreatePlatformFile(
        file->path,
        base::PLATFORM_FILE_READ | base::PLATFORM_FILE_OPEN,
        NULL,
        NULL);
  } else {
    file_util::Delete(file->path, false);
  }

  return file.Pass();
}

// Saves |data| to file at |path|. Returns true on successful save, otherwise
// returns false.
bool SaveDictionaryData(scoped_ptr<std::string> data,
                        const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  size_t bytes_written =
      file_util::WriteFile(path, data->data(), data->length());
  if (bytes_written != data->length()) {
    bool success = false;
#if defined(OS_WIN)
    base::FilePath dict_dir;
    PathService::Get(chrome::DIR_USER_DATA, &dict_dir);
    base::FilePath fallback_file_path =
        dict_dir.Append(path.BaseName());
    bytes_written =
        file_util::WriteFile(fallback_file_path, data->data(), data->length());
    if (bytes_written == data->length())
      success = true;
#endif

    if (!success) {
      file_util::Delete(path, false);
      return false;
    }
  }

  return true;
}

}  // namespace

SpellcheckHunspellDictionary::SpellcheckHunspellDictionary(
    const std::string& language,
    net::URLRequestContextGetter* request_context_getter,
    SpellcheckService* spellcheck_service)
    : language_(language),
      use_platform_spellchecker_(false),
      request_context_getter_(request_context_getter),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      spellcheck_service_(spellcheck_service),
      download_status_(DOWNLOAD_NONE),
      dictionary_file_(new DictionaryFile) {
}

SpellcheckHunspellDictionary::~SpellcheckHunspellDictionary() {
}

void SpellcheckHunspellDictionary::Load() {
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

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&InitializeDictionaryLocation, language_),
      base::Bind(
          &SpellcheckHunspellDictionary::InitializeDictionaryLocationComplete,
          weak_ptr_factory_.GetWeakPtr()));
}

void SpellcheckHunspellDictionary::RetryDownloadDictionary(
      net::URLRequestContextGetter* request_context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  request_context_getter_ = request_context_getter;
  DownloadDictionary();
}

bool SpellcheckHunspellDictionary::IsReady() const {
  return GetDictionaryFile() !=
      base::kInvalidPlatformFileValue || IsUsingPlatformChecker();
}

const base::PlatformFile&
SpellcheckHunspellDictionary::GetDictionaryFile() const {
  return dictionary_file_->descriptor;
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

void SpellcheckHunspellDictionary::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(source);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<net::URLFetcher> fetcher_destructor(fetcher_.release());

  if ((source->GetResponseCode() / 100) != 2) {
    // Initialize will not try to download the file a second time.
    InformListenersOfDownloadFailure();
    return;
  }

  // Basic sanity check on the dictionary. There's a small chance of 200 status
  // code for a body that represents some form of failure.
  scoped_ptr<std::string> data(new std::string);
  source->GetResponseAsString(data.get());
  if (data->size() < 4 || data->compare(0, 4, "BDic") != 0) {
    InformListenersOfDownloadFailure();
    return;
  }

  // To prevent corrupted dictionary data from causing a renderer crash, scan
  // the dictionary data and verify it is sane before save it to a file.
  // TODO(rlp): Adding metrics to RecordDictionaryCorruptionStats
  if (!hunspell::BDict::Verify(data->data(), data->size())) {
    // Let PostTaskAndReply caller send to InformListenersOfInitialization
    // through SaveDictionaryDataComplete().
    SaveDictionaryDataComplete(false);
    return;
  }

  BrowserThread::PostTaskAndReplyWithResult<bool>(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&SaveDictionaryData,
                 base::Passed(&data),
                 dictionary_file_->path),
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
  std::string bdict_file = dictionary_file_->path.BaseName().MaybeAsASCII();
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
  fetcher_->Start();
  // Attempt downloading the dictionary only once.
  request_context_getter_ = NULL;
}

void SpellcheckHunspellDictionary::InitializeDictionaryLocationComplete(
    scoped_ptr<DictionaryFile> file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  dictionary_file_ = file.Pass();

  if (dictionary_file_->descriptor == base::kInvalidPlatformFileValue) {

    // Notify browser tests that this dictionary is corrupted. Skip downloading
    // the dictionary in browser tests.
    // TODO(rouslan): Remove this test-only case.
    if (spellcheck_service_->SignalStatusEvent(
          SpellcheckService::BDICT_CORRUPTED)) {
      request_context_getter_ = NULL;
    }

    if (request_context_getter_) {
      // Download from the UI thread to check that |request_context_getter_| is
      // still valid.
      DownloadDictionary();
      return;
    }
  }

  InformListenersOfInitialization();
}

void SpellcheckHunspellDictionary::SaveDictionaryDataComplete(
    bool dictionary_saved) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (dictionary_saved) {
    download_status_ = DOWNLOAD_NONE;
    FOR_EACH_OBSERVER(Observer,
                      observers_,
                      OnHunspellDictionaryDownloadSuccess());
    Load();
  } else {
    InformListenersOfDownloadFailure();
    InformListenersOfInitialization();
  }
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
