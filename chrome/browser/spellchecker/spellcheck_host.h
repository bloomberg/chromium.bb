// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HOST_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HOST_H_
#pragma once

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "content/public/browser/browser_thread.h"

namespace base {
class WaitableEvent;
}

namespace content {
class RenderProcessHost;
}

class Profile;
class SpellCheckHostMetrics;
class SpellCheckProfileProvider;

namespace net {
class URLRequestContextGetter;
}

// An abstract interface that provides operations that controls the spellchecker
// attached to the browser. This class provides the operations listed below:
// * Adding a word to the user dictionary;
// * Retrieving the dictionary file (if it has one);
// * Retrieving the list of words in the user dictionary;
// * Retrieving the language used by the spellchecker.
// * Listing available languages for a Profile object;
// * Accepting an observer to reacting the state change of the object.
//   You can also remove the observer from the SpellCheckHost object.
//   The object should implement SpellCheckProfileProvider interface.
//
// The following snippet describes how to use this class,
//   std::vector<std::string> languages;
//   SpellCheckHost::GetSpellCheckLanguages(profile_, &languages);
//   spellcheck_host_ = SpellCheckHost::Create(
//       observer, languages[0], req_getter);
//
// The class is intended to be owned by SpellCheckProfile so users should
// retrieve the instance via Profile::GetSpellCheckHost().
// Users should not hold the reference over the function scope because
// the instance can be invalidated during the browser's lifecycle.
class SpellCheckHost {
 public:
  // Event types used for reporting the status of this class and its derived
  // classes to browser tests.
  enum EventType {
    BDICT_NOTINITIALIZED,
    BDICT_CORRUPTED,
  };

  virtual ~SpellCheckHost() {}

  // Creates the instance of SpellCheckHost implementation object.
  static SpellCheckHost* Create(
      SpellCheckProfileProvider* profile,
      const std::string& language,
      net::URLRequestContextGetter* request_context_getter,
      SpellCheckHostMetrics* metrics);

  // Clears a profile which is set on creation.
  // Used to prevent calling back to a deleted object.
  virtual void UnsetProfile() = 0;

  // Pass the renderer some basic intialization information. Note that the
  // renderer will not load Hunspell until it needs to.
  virtual void InitForRenderer(content::RenderProcessHost* process) = 0;

  // Adds the given word to the custom words list and inform renderer of the
  // update.
  virtual void AddWord(const std::string& word) = 0;

  virtual const base::PlatformFile& GetDictionaryFile() const = 0;

  virtual const std::string& GetLanguage() const = 0;

  virtual bool IsUsingPlatformChecker() const = 0;

  // Returns a metrics counter associated with this object,
  // or null when metrics recording is disabled.
  virtual SpellCheckHostMetrics* GetMetrics() const = 0;

  // Returns true if the dictionary is ready to use.
  virtual bool IsReady() const = 0;

  // This function computes a vector of strings which are to be displayed in
  // the context menu over a text area for changing spell check languages. It
  // returns the index of the current spell check language in the vector.
  // TODO(port): this should take a vector of string16, but the implementation
  // has some dependencies in l10n util that need porting first.
  static int GetSpellCheckLanguages(Profile* profile,
                                    std::vector<std::string>* languages);

 protected:
  // Signals the event attached by AttachTestEvent() to report the specified
  // event to browser tests. This function is called by this class and its
  // derived classes to report their status. This function does not do anything
  // when we do not set an event to |status_event_|.
  static bool SignalStatusEvent(EventType type);

 private:
  FRIEND_TEST_ALL_PREFIXES(SpellCheckHostBrowserTest, DeleteCorruptedBDICT);

  // Attaches an event so browser tests can listen the status events.
  static void AttachStatusEvent(base::WaitableEvent* status_event);

  // Waits until a spellchecker updates its status. This function returns
  // immediately when we do not set an event to |status_event_|.
  static EventType WaitStatusEvent();
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_HOST_H_
