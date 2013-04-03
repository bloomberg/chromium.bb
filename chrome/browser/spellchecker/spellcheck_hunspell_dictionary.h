// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HUNSPELL_DICTIONARY_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HUNSPELL_DICTIONARY_H_

#include "chrome/browser/spellchecker/spellcheck_dictionary.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/platform_file.h"
#include "net/url_request/url_fetcher_delegate.h"

class GURL;
class Profile;
class SpellcheckService;
struct DictionaryFile;

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

// Defines the browser-side hunspell dictionary and provides access to it.
class SpellcheckHunspellDictionary
    : public SpellcheckDictionary,
      public net::URLFetcherDelegate,
      public base::SupportsWeakPtr<SpellcheckHunspellDictionary> {
 public:
  // Interface to implement for observers of the Hunspell dictionary.
  class Observer {
   public:
    // The dictionary has been initialized.
    virtual void OnHunspellDictionaryInitialized() = 0;

    // Dictionary download began.
    virtual void OnHunspellDictionaryDownloadBegin() = 0;

    // Dictionary download succeeded.
    virtual void OnHunspellDictionaryDownloadSuccess() = 0;

    // Dictionary download failed.
    virtual void OnHunspellDictionaryDownloadFailure() = 0;
  };

  SpellcheckHunspellDictionary(
      const std::string& language,
      net::URLRequestContextGetter* request_context_getter,
      SpellcheckService* spellcheck_service);
  virtual ~SpellcheckHunspellDictionary();

  // SpellcheckDictionary implementation:
  virtual void Load() OVERRIDE;

  // Retry downloading |dictionary_file_|.
  void RetryDownloadDictionary(
      net::URLRequestContextGetter* request_context_getter);

  // Returns true if the dictionary is ready to use.
  virtual bool IsReady() const;

  // TODO(rlp): Return by value.
  const base::PlatformFile& GetDictionaryFile() const;
  const std::string& GetLanguage() const;
  bool IsUsingPlatformChecker() const;

  // Add an observer for Hunspell dictionary events.
  void AddObserver(Observer* observer);

  // Remove an observer for Hunspell dictionary events.
  void RemoveObserver(Observer* observer);

  // Whether dictionary is being downloaded.
  bool IsDownloadInProgress();

  // Whether dictionary download failed.
  bool IsDownloadFailure();

 private:
  // Dictionary download status.
  enum DownloadStatus {
    DOWNLOAD_NONE,
    DOWNLOAD_IN_PROGRESS,
    DOWNLOAD_FAILED,
  };

  // net::URLFetcherDelegate implementation. Called when dictionary download
  // finishes.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Determine the correct url to download the dictionary.
  GURL GetDictionaryURL();

  // Attempt to download the dictionary.
  void DownloadDictionary(GURL url);

  // The reply point for PostTaskAndReplyWithResult, called after the dictionary
  // file has been initialized.
  void InitializeDictionaryLocationComplete(scoped_ptr<DictionaryFile> file);

  // The reply point for PostTaskAndReplyWithResult, called after the dictionary
  // file has been saved.
  void SaveDictionaryDataComplete(bool dictionary_saved);

  // Notify listeners that the dictionary has been initialized.
  void InformListenersOfInitialization();

  // Notify listeners that the dictionary download failed.
  void InformListenersOfDownloadFailure();

  // The language of the dictionary file.
  std::string language_;

  // Whether to use the platform spellchecker instead of Hunspell.
  bool use_platform_spellchecker_;

  // Used for downloading the dictionary file. SpellcheckHunspellDictionary does
  // not hold a reference, and it is only valid to use it on the UI thread.
  net::URLRequestContextGetter* request_context_getter_;

  // Used for downloading the dictionary file.
  scoped_ptr<net::URLFetcher> fetcher_;

  base::WeakPtrFactory<SpellcheckHunspellDictionary> weak_ptr_factory_;

  SpellcheckService* spellcheck_service_;

  // Observers of Hunspell dictionary events.
  ObserverList<Observer> observers_;

  // Status of the dictionary download.
  DownloadStatus download_status_;

  // Dictionary file path and descriptor.
  scoped_ptr<DictionaryFile> dictionary_file_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckHunspellDictionary);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HUNSPELL_DICTIONARY_H_
