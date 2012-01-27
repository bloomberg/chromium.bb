// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_PROFILE_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_PROFILE_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/spellchecker/spellcheck_profile_provider.h"
#include "content/public/browser/notification_observer.h"

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
// The class is intended to be owned and managed by SpellCheckFactory
// internally.  Any usable APIs are provided by SpellCheckHost interface, which
// can be retrieved from SpellCheckFactory::GetHostForProfile();
class SpellCheckProfile : public SpellCheckProfileProvider,
                          public ProfileKeyedService,
                          public content::NotificationObserver {
 public:
  explicit SpellCheckProfile(Profile* profile);
  virtual ~SpellCheckProfile();

  // Retrieves SpellCheckHost object.
  // This can return NULL when the spell-checking is disabled
  // or the host object is not ready yet.
  SpellCheckHost* GetHost();

  // If |force| is false, and the spellchecker is already initialized (or is in
  // the process of initializing), then do nothing. Otherwise clobber the
  // current spellchecker and replace it with a new one.
  void ReinitializeSpellCheckHost(bool force);

  // Instantiates SpellCheckHostMetrics object and
  // makes it ready for recording metrics.
  // This should be called only if the metrics recording is active.
  void StartRecordingMetrics(bool spellcheck_enabled);

  // SpellCheckProfileProvider implementation.
  virtual void SpellCheckHostInitialized(CustomWordList* custom_words) OVERRIDE;
  virtual const CustomWordList& GetCustomWords() const OVERRIDE;
  virtual void CustomWordAddedLocally(const std::string& word) OVERRIDE;
  virtual void LoadCustomDictionary(CustomWordList* custom_words) OVERRIDE;
  virtual void WriteWordToCustomDictionary(const std::string& word) OVERRIDE;

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  // Only tests should override this.
  virtual SpellCheckHost* CreateHost(
      SpellCheckProfileProvider* provider,
      const std::string& language,
      net::URLRequestContextGetter* request_context,
      SpellCheckHostMetrics* metrics);

  virtual bool IsTesting() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SpellCheckProfileTest, ReinitializeEnabled);
  FRIEND_TEST_ALL_PREFIXES(SpellCheckProfileTest, ReinitializeDisabled);
  FRIEND_TEST_ALL_PREFIXES(SpellCheckProfileTest, ReinitializeRemove);
  FRIEND_TEST_ALL_PREFIXES(SpellCheckProfileTest, ReinitializeRecreate);
  FRIEND_TEST_ALL_PREFIXES(SpellCheckProfileTest,
                           SpellCheckHostInitializedWithCustomWords);
  FRIEND_TEST_ALL_PREFIXES(SpellCheckProfileTest, CustomWordAddedLocally);
  FRIEND_TEST_ALL_PREFIXES(SpellCheckProfileTest, SaveAndLoad);
  FRIEND_TEST_ALL_PREFIXES(SpellCheckProfileTest, MultiProfile);

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

  // Initializes or deletes SpellCheckHost object if necessary.
  // The caller should provide URLRequestContextGetter for
  // possible dictionary download.
  //
  // If |force| is true, the host instance is newly created
  // regardless there is existing instance.
  ReinitializeResult ReinitializeHostImpl(
      bool force,
      bool enable,
      const std::string& language,
      net::URLRequestContextGetter* request_context);

  const FilePath& GetCustomDictionaryPath();

  PrefChangeRegistrar pref_change_registrar_;

  Profile* profile_;

  scoped_ptr<SpellCheckHost> host_;
  scoped_ptr<SpellCheckHostMetrics> metrics_;

  // Indicates whether |host_| has told us initialization is
  // finished.
  bool host_ready_;

  // In-memory cache of the custom words file.
  scoped_ptr<CustomWordList> custom_words_;

  // A directory path of profile.
  FilePath profile_dir_;

  // A path for custom dictionary per profile.
  scoped_ptr<FilePath> custom_dictionary_path_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckProfile);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECK_PROFILE_H_
