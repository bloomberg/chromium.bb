// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HOST_IMPL_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HOST_IMPL_H_
#pragma once

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/spellchecker/spellcheck_host.h"
#include "chrome/browser/spellchecker/spellcheck_host_observer.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "content/common/url_fetcher.h"

// This class implements the SpellCheckHost interface to provide the
// functionalities listed below:
// * Adding a word to the custom dictionary;
// * Storing the custom dictionary to the file, and read it back
//   from the file during the initialization.
// * Downloading a dictionary and map it for the renderer, and;
// * Calling the platform spellchecker attached to the browser;
//
// To download a dictionary and initialize it without blocking the UI thread,
// this class also implements the URLFetcher::Delegate() interface. This
// initialization status is notified to the UI thread through the
// SpellCheckHostObserver interface.
//
// We expect a profile creates an instance of this class through a factory
// method, SpellCheckHost::Create() and uses only the SpellCheckHost interface
// provided by this class.
//   spell_check_host_ = SpellCheckHost::Create(...)
//
// Available languages for the checker, which we need to specify via Create(),
// can be listed using SpellCheckHost::GetAvailableLanguages() static method.
class SpellCheckHostImpl : public SpellCheckHost,
                           public URLFetcher::Delegate,
                           public NotificationObserver {
 public:
  SpellCheckHostImpl(SpellCheckHostObserver* observer,
                     const std::string& language,
                     net::URLRequestContextGetter* request_context_getter,
                     SpellCheckHostMetrics* metrics);

  void Initialize();

  // SpellCheckHost implementation
  virtual void UnsetObserver();
  virtual void InitForRenderer(RenderProcessHost* process);
  virtual void AddWord(const std::string& word);
  virtual const base::PlatformFile& GetDictionaryFile() const;
  virtual const std::string& GetLanguage() const;
  virtual bool IsUsingPlatformChecker() const;

 private:
  typedef SpellCheckHostObserver::CustomWordList CustomWordList;

  // These two classes can destruct us.
  friend class BrowserThread;
  friend class DeleteTask<SpellCheckHostImpl>;

  virtual ~SpellCheckHostImpl();

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
  // |custom_words| holds the custom word list which was
  // loaded at the file thread.
  void InformObserverOfInitializationWithCustomWords(
      CustomWordList* custom_words);

  // An alternative version of InformObserverOfInitializationWithCustomWords()
  // which implies empty |custom_words|.
  void InformObserverOfInitialization();

  // If |dictionary_file_| is missing, we attempt to download it.
  void DownloadDictionary();

  // Write a custom dictionary addition to disk.
  void WriteWordToCustomDictionary(const std::string& word);

  // Returns a metrics counter associated with this object,
  // or null when metrics recording is disabled.
  virtual SpellCheckHostMetrics* GetMetrics() const;

  // Returns true if the dictionary is ready to use.
  virtual bool IsReady() const;

  // URLFetcher::Delegate implementation.  Called when we finish downloading the
  // spellcheck dictionary; saves the dictionary to |data_|.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const net::URLRequestStatus& status,
                                  int response_code,
                                  const net::ResponseCookies& cookies,
                                  const std::string& data);

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Saves |data_| to disk. Run on the file thread.
  void SaveDictionaryData();

  // Verifies the specified BDict file exists and it is sane. This function
  // should be called before opening the file so we can delete it and download a
  // new dictionary if it is corrupted.
  bool VerifyBDict(const FilePath& path) const;

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

  // We don't want to attempt to download a missing dictionary file more than
  // once.
  bool tried_to_download_;

  // Whether we should use the platform spellchecker instead of Hunspell.
  bool use_platform_spellchecker_;

  // Data received from the dictionary download.
  std::string data_;

  // Used for downloading the dictionary file. We don't hold a reference, and
  // it is only valid to use it on the UI thread.
  net::URLRequestContextGetter* request_context_getter_;

  // Used for downloading the dictionary file.
  scoped_ptr<URLFetcher> fetcher_;

  NotificationRegistrar registrar_;

  // An optional metrics counter given by the constructor.
  SpellCheckHostMetrics* metrics_;
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HOST_IMPL_H_
