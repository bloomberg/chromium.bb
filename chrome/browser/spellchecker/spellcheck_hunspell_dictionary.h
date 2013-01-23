// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HUNSPELL_DICTIONARY_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HUNSPELL_DICTIONARY_H_

#include "chrome/browser/spellchecker/spellcheck_dictionary.h"

#include "base/file_path.h"
#include "base/platform_file.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_fetcher_delegate.h"

class Profile;
class SpellcheckService;

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

// Defines the browser-side hunspell dictionary and provides access to it.
class SpellcheckHunspellDictionary
    : public SpellcheckDictionary,
      public net::URLFetcherDelegate {
 public:
  // TODO(rlp): Passing in the host is very temporary. In the next CL this
  // will be removed.
  SpellcheckHunspellDictionary(
      Profile* profile,
      const std::string& language,
      net::URLRequestContextGetter* request_context_getter,
      SpellcheckService* spellcheck_service);
  virtual ~SpellcheckHunspellDictionary();

  virtual void Load() OVERRIDE;

  void Initialize();

  // Figure out the location for the dictionary. This is only non-trivial for
  // Windows:
  // The default place whether the spellcheck dictionary can reside is
  // chrome::DIR_APP_DICTIONARIES. However, for systemwide installations,
  // this directory may not have permissions for download. In that case, the
  // alternate directory for download is chrome::DIR_USER_DATA.
  void InitializeDictionaryLocation();
  void InitializeDictionaryLocationComplete();

  // net::URLFetcherDelegate implementation.  Called when we finish
  // downloading the spellcheck dictionary; saves the dictionary to |data_|.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // If |dictionary_file_| is missing, we attempt to download it.
  void DownloadDictionary();

  // Saves |data_| to disk. Run on the file thread.
  void SaveDictionaryData(std::string* data);
  void SaveDictionaryDataComplete();

  // Verifies the specified BDict file exists and it is sane. This function
  // should be called before opening the file so we can delete it and download a
  // new dictionary if it is corrupted.
  bool VerifyBDict(const FilePath& path) const;

  // Returns true if the dictionary is ready to use.
  virtual bool IsReady() const;

  void InformProfileOfInitialization();

  // TODO(rlp): Return by value.
  const base::PlatformFile& GetDictionaryFile() const;
  const std::string& GetLanguage() const;
  bool IsUsingPlatformChecker() const;

 private:
  // The desired location of the dictionary file, whether or not it exists yet.
  FilePath bdict_file_path_;

  // State whether a dictionary has been partially, or fully saved. If the
  // former, shortcut Initialize.
  bool dictionary_saved_;

  // The language of the dictionary file.
  std::string language_;

  // The file descriptor/handle for the dictionary file.
  base::PlatformFile file_;

  // We don't want to attempt to download a missing dictionary file more than
  // once.
  bool tried_to_download_;

  // Whether we should use the platform spellchecker instead of Hunspell.
  bool use_platform_spellchecker_;

  // Used for downloading the dictionary file. We don't hold a reference, and
  // it is only valid to use it on the UI thread.
  net::URLRequestContextGetter* request_context_getter_;

  // Used for downloading the dictionary file.
  scoped_ptr<net::URLFetcher> fetcher_;

  base::WeakPtrFactory<SpellcheckHunspellDictionary> weak_ptr_factory_;

  SpellcheckService* spellcheck_service_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckHunspellDictionary);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HUNSPELL_DICTIONARY_H_
