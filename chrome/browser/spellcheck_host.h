// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECK_HOST_H_
#define CHROME_BROWSER_SPELLCHECK_HOST_H_
#pragma once

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/platform_file.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/common/net/url_fetcher.h"
#include "content/browser/browser_thread.h"

class Profile;
class SpellCheckHostObserver;
class URLRequestContextGetter;

class SpellCheckHost : public base::RefCountedThreadSafe<SpellCheckHost,
                           BrowserThread::DeleteOnFileThread>,
                       public URLFetcher::Delegate {
 public:
  SpellCheckHost(SpellCheckHostObserver* observer,
                 const std::string& language,
                 URLRequestContextGetter* request_context_getter);

  void Initialize();

  // Clear |observer_|. Used to prevent calling back to a deleted object.
  void UnsetObserver();

  // Add the given word to the custom words list and inform renderer of the
  // update.
  void AddWord(const std::string& word);

  // This function computes a vector of strings which are to be displayed in
  // the context menu over a text area for changing spell check languages. It
  // returns the index of the current spell check language in the vector.
  // TODO(port): this should take a vector of string16, but the implementation
  // has some dependencies in l10n util that need porting first.
  static int GetSpellCheckLanguages(Profile* profile,
                                    std::vector<std::string>* languages);

  const base::PlatformFile& bdict_file() const { return file_; }

  const std::vector<std::string>& custom_words() const { return custom_words_; }

  const std::string& last_added_word() const { return custom_words_.back(); }

  const std::string& language() const { return language_; }

  bool use_platform_spellchecker() const { return use_platform_spellchecker_; }

 private:
  // These two classes can destruct us.
  friend class BrowserThread;
  friend class DeleteTask<SpellCheckHost>;

  virtual ~SpellCheckHost();

  // Figure out the location for the dictionary. This is only non-trivial for
  // Windows:
  // The default place whether the spellcheck dictionary can reside is
  // chrome::DIR_APP_DICTIONARIES. However, for systemwide installations,
  // this directory may not have permissions for download. In that case, the
  // alternate directory for download is chrome::DIR_USER_DATA.
  void InitializeDictionaryLocation();

  // Load and parse the custom words dictionary and open the bdic file.
  // Executed on the file thread.
  void InitializeInternal();

  void InitializeOnFileThread();

  // Inform |observer_| that initialization has finished.
  void InformObserverOfInitialization();

  // If |bdict_file_| is missing, we attempt to download it.
  void DownloadDictionary();

  // Write a custom dictionary addition to disk.
  void WriteWordToCustomDictionary(const std::string& word);

  // URLFetcher::Delegate implementation.  Called when we finish downloading the
  // spellcheck dictionary; saves the dictionary to |data_|.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // Saves |data_| to disk. Run on the file thread.
  void SaveDictionaryData();

  // May be NULL.
  SpellCheckHostObserver* observer_;

  // The desired location of the dictionary file (whether or not t exists yet).
  FilePath bdict_file_path_;

  // The location of the custom words file.
  FilePath custom_dictionary_file_;

  // The language of the dictionary file.
  std::string language_;

  // The file descriptor/handle for the dictionary file.
  base::PlatformFile file_;

  // In-memory cache of the custom words file.
  std::vector<std::string> custom_words_;

  // We don't want to attempt to download a missing dictionary file more than
  // once.
  bool tried_to_download_;

  // Whether we should use the platform spellchecker instead of Hunspell.
  bool use_platform_spellchecker_;

  // Data received from the dictionary download.
  std::string data_;

  // Used for downloading the dictionary file. We don't hold a reference, and
  // it is only valid to use it on the UI thread.
  URLRequestContextGetter* request_context_getter_;

  // Used for downloading the dictionary file.
  scoped_ptr<URLFetcher> fetcher_;
};

#endif  // CHROME_BROWSER_SPELLCHECK_HOST_H_
