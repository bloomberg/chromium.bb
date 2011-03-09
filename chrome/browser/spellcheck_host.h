// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECK_HOST_H_
#define CHROME_BROWSER_SPELLCHECK_HOST_H_
#pragma once

#include <string>
#include <vector>

#include "base/platform_file.h"
#include "base/ref_counted.h"
#include "content/browser/browser_thread.h"

class Profile;
class SpellCheckHostObserver;
class URLRequestContextGetter;

// An abstract interface that provides operations that controls the spellchecker
// attached to the browser. This class provides the operations listed below:
// * Adding a word to the user dictionary;
// * Retrieving the dictionary file (if it has one);
// * Retrieving the list of words in the user dictionary;
// * Retrieving the language used by the spellchecker.
// * Listing available languages for a Profile object;
// * Accepting an observer to reacting the state change of the object.
//   You can also remove the observer from the SpellCheckHost object.
//   The object should implement SpellCheckHostObserver interface.
//
// The following snippet describes how to use this class,
//   std::vector<std::string> languages;
//   SpellCheckHost::GetSpellCheckLanguages(profile_, &languages);
//   spellcheck_host_ = SpellCheckHost::Create(
//       observer, languages[0], req_getter);
//
// The class is intended to be owned by ProfileImpl so users should
// retrieve the instance via Profile::GetSpellCheckHost().
// Users should not hold the reference over the function scope because
// the instance can be invalidated during the browser's lifecycle.
class SpellCheckHost
    : public base::RefCountedThreadSafe<SpellCheckHost,
                                        BrowserThread::DeleteOnFileThread> {
 public:
  virtual ~SpellCheckHost() {}

  // Creates the instance of SpellCheckHost implementation object.
  static scoped_refptr<SpellCheckHost> Create(
      SpellCheckHostObserver* observer,
      const std::string& language,
      URLRequestContextGetter* request_context_getter);

  // Clears an observer which is set on creation.
  // Used to prevent calling back to a deleted object.
  virtual void UnsetObserver() = 0;

  // Adds the given word to the custom words list and inform renderer of the
  // update.
  virtual void AddWord(const std::string& word) = 0;

  virtual const base::PlatformFile& GetDictionaryFile() const = 0;

  virtual const std::vector<std::string>& GetCustomWords() const = 0;

  virtual const std::string& GetLastAddedFile() const = 0;

  virtual const std::string& GetLanguage() const = 0;

  virtual bool IsUsingPlatformChecker() const = 0;

  // This function computes a vector of strings which are to be displayed in
  // the context menu over a text area for changing spell check languages. It
  // returns the index of the current spell check language in the vector.
  // TODO(port): this should take a vector of string16, but the implementation
  // has some dependencies in l10n util that need porting first.
  static int GetSpellCheckLanguages(Profile* profile,
                                    std::vector<std::string>* languages);
};

#endif  // CHROME_BROWSER_SPELLCHECK_HOST_H_
