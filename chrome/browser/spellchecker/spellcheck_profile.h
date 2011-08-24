// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_PROFILE_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_PROFILE_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/spellchecker/spellcheck_profile_provider.h"

class Profile;
class SpellCheckHost;
class SpellCheckHostMetrics;

namespace net {
class URLRequestContextGetter;
}

// A facade of spell-check related objects in the browser process.
// This class is responsible for managing a life-cycle of:
// * SpellCheckHost object (instantiation, deletion, reset)
// * SpellCheckHostMetrics object (only initiate when metrics is enabled)
//
// The following snippet describes how to use this class:
//
//   PrefService* pref = ....;
//   net::URLRequestContextGetter* context = ...;
//   SpellCheckProfile* profile = new SpellCheckProfile();
//   profile->StartRecordingMetrics(true); // to enable recording
//   profile->ReinitializeResult(true, true, pref->GetLanguage(), context);
//   ...
//   SpellCheckHost* host = profile->GetHost();
//
// The class is intended to be owned and managed by ProfileImpl internally.
// Any usable APIs are provided by SpellCheckHost interface,
// which can be retrieved from Profile::GetSpellCheckHost().
class SpellCheckProfile : public SpellCheckProfileProvider {
 public:
  // Return value of ReinitializeHost()
  enum ReinitializeResult {
    // SpellCheckProfile created new SpellCheckHost object. We can
    // retrieve it once the asynchronous initialization task is
    // finished.
    REINITIALIZE_CREATED_HOST,
    // An existing SpellCheckHost insntace is deleted, that means
    // the spell-check feature just tunred from enabled to disabled.
    // The state should be synced to renderer processes
    REINITIALIZE_REMOVED_HOST,
    // The API did nothing. SpellCheckHost instance isn't created nor
    // deleted.
    REINITIALIZE_DID_NOTHING
  };

  SpellCheckProfile();
  virtual ~SpellCheckProfile();

  // Retrieves SpellCheckHost object.
  // This can return NULL when the spell-checking is disabled
  // or the host object is not ready yet.
  SpellCheckHost* GetHost();

  // Initializes or deletes SpellCheckHost object if necessary.
  // The caller should provide URLRequestContextGetter for
  // possible dictionary download.
  //
  // If |force| is true, the host instance is newly created
  // regardless there is existing instance.
  ReinitializeResult ReinitializeHost(
      bool force,
      bool enable,
      const std::string& language,
      net::URLRequestContextGetter* request_context);

  // Instantiates SpellCheckHostMetrics object and
  // makes it ready for recording metrics.
  // This should be called only if the metrics recording is active.
  void StartRecordingMetrics(bool spellcheck_enabled);

  // SpellCheckProfileProvider implementation.
  virtual void SpellCheckHostInitialized(CustomWordList* custom_words);
  virtual const CustomWordList& GetCustomWords() const;
  virtual void CustomWordAddedLocally(const std::string& word);

 protected:
  // Only tests should override this.
  virtual SpellCheckHost* CreateHost(
      SpellCheckProfileProvider* provider,
      const std::string& language,
      net::URLRequestContextGetter* request_context,
      SpellCheckHostMetrics* metrics);

  virtual bool IsTesting() const;

 private:
  scoped_refptr<SpellCheckHost> host_;
  scoped_ptr<SpellCheckHostMetrics> metrics_;

  // Indicates whether |host_| has told us initialization is
  // finished.
  bool host_ready_;

  // In-memory cache of the custom words file.
  scoped_ptr<CustomWordList> custom_words_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckProfile);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_PROFILE_H_
