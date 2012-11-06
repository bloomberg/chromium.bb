// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HOST_IMPL_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HOST_IMPL_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_host.h"
#include "chrome/browser/spellchecker/spellcheck_hunspell_dictionary.h"
#include "chrome/browser/spellchecker/spellcheck_profile_provider.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLFetcher;
}  // namespace net

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
// SpellCheckProfileProvider interface.
//
// We expect a profile creates an instance of this class through a factory
// method, SpellCheckHost::Create() and uses only the SpellCheckHost interface
// provided by this class.
//   spell_check_host_ = SpellCheckHost::Create(...)
//
// Available languages for the checker, which we need to specify via Create(),
// can be listed using SpellCheckHost::GetAvailableLanguages() static method.
class SpellCheckHostImpl : public SpellCheckHost,
                           public content::NotificationObserver {
 public:
  SpellCheckHostImpl(SpellCheckProfileProvider* profile,
                     const std::string& language,
                     net::URLRequestContextGetter* request_context_getter,
                     SpellCheckHostMetrics* metrics);

  virtual ~SpellCheckHostImpl();

  void Initialize();

  // An alternative version of InformProfileOfInitializationWithCustomWords()
  // which implies empty |custom_words|.
  void InformProfileOfInitialization();

  // SpellCheckHost implementation
  virtual void UnsetProfile() OVERRIDE;
  virtual void InitForRenderer(content::RenderProcessHost* process) OVERRIDE;
  virtual void AddWord(const std::string& word) OVERRIDE;
  virtual const base::PlatformFile& GetDictionaryFile() const OVERRIDE;
  virtual const std::string& GetLanguage() const OVERRIDE;
  virtual bool IsUsingPlatformChecker() const OVERRIDE;

 private:
  typedef chrome::spellcheck_common::WordList CustomWordList;

  // These two classes can destruct us.
  friend class content::BrowserThread;
  friend class base::DeleteHelper<SpellCheckHostImpl>;

  // The reply point for PostTaskAndReply. Called when AddWord is finished
  // adding a word in the background.
  void AddWordComplete(const std::string& word);

  // Inform |profile_| that initialization has finished.
  // |custom_words| holds the custom word list which was
  // loaded at the file thread.
  void InformProfileOfInitializationWithCustomWords(
      CustomWordList* custom_words);

  // Loads a custom dictionary from disk.
  void LoadCustomDictionary(CustomWordList* custom_words);

  // Write a custom dictionary addition to disk.
  void WriteWordToCustomDictionary(const std::string& word);

  // Returns a metrics counter associated with this object,
  // or null when metrics recording is disabled.
  virtual SpellCheckHostMetrics* GetMetrics() const OVERRIDE;

  // Returns true if the dictionary is ready to use.
  virtual bool IsReady() const OVERRIDE;

  // NotificationProfile implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // May be NULL.
  SpellCheckProfileProvider* profile_;

  content::NotificationRegistrar registrar_;

  // An optional metrics counter given by the constructor.
  SpellCheckHostMetrics* metrics_;

  base::WeakPtrFactory<SpellCheckHostImpl> weak_ptr_factory_;

  scoped_ptr<SpellcheckHunspellDictionary> hunspell_dictionary_;

  scoped_ptr<CustomWordList> custom_words_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckHostImpl);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HOST_IMPL_H_
