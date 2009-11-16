// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECK_HOST_H_
#define CHROME_BROWSER_SPELLCHECK_HOST_H_

#include <string>
#include <vector>

#include "base/file_descriptor_posix.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/browser/net/url_request_context_getter.h"

class SpellCheckHost : public base::RefCountedThreadSafe<SpellCheckHost,
                           ChromeThread::DeleteOnFileThread>,
                       public URLFetcher::Delegate {
 public:
  class Observer {
   public:
    virtual void SpellCheckHostInitialized() = 0;
  };

  SpellCheckHost(Observer* observer,
                 const std::string& language,
                 URLRequestContextGetter* request_context_getter);

  void Initialize();

  // Clear |observer_|. Used to prevent calling back to a deleted object.
  void UnsetObserver();

  // Add the given word to the custom words list and inform renderer of the
  // update.
  void AddWord(const std::string& word);

  const base::FileDescriptor& bdict_fd() const { return fd_; };

  const std::vector<std::string>& custom_words() const { return custom_words_; }

  const std::string& last_added_word() const { return custom_words_.back(); }

  const std::string& language() const { return language_; }

 private:
  // These two classes can destruct us.
  friend class ChromeThread;
  friend class DeleteTask<SpellCheckHost>;

  virtual ~SpellCheckHost();

  // Load and parse the custom words dictionary and open the bdic file.
  // Executed on the file thread.
  void InitializeInternal();

  // Inform |observer_| that initialization has finished.
  void InformObserverOfInitialization();

  // If |bdict_file_| is missing, we attempt to download it.
  void DownloadDictionary();

  // Write a custom dictionary addition to disk.
  void WriteWordToCustomDictionary(const std::string& word);

  // URLFetcher::Delegate implementation.  Called when we finish downloading the
  // spellcheck dictionary; saves the dictionary to disk.
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // May be NULL.
  Observer* observer_;

  // The desired location of the dictionary file (whether or not it exists yet).
  FilePath bdict_file_;

  // The location of the custom words file.
  FilePath custom_dictionary_file_;

  // The language of the dictionary file.
  std::string language_;

  // On POSIX, the file descriptor for the dictionary file.
  base::FileDescriptor fd_;

  // In-memory cache of the custom words file.
  std::vector<std::string> custom_words_;

  // We don't want to attempt to download a missing dictionary file more than
  // once.
  bool tried_to_download_;

  // Used for downloading the dictionary file.
  scoped_refptr<URLRequestContextGetter> request_context_getter_;

  // Used for downloading the dictionary file.
  scoped_ptr<URLFetcher> fetcher_;
};

#endif  // CHROME_BROWSER_SPELLCHECK_HOST_H_
