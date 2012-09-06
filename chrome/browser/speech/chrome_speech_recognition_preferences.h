// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_PREFERENCES_H_
#define CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_PREFERENCES_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/api/prefs/pref_change_registrar.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/speech_recognition_preferences.h"

class PrefService;

namespace base {
class ListValue;
}

// Utility class that handles persistent storage of speech recognition
// preferences. Instances of this class must be obtained exclusively through the
// method ChromeSpeechRecognitionPreferences::GetForProfile(...), so that
// preferences are handled within the Profile (if not NULL). The internal
// factory class also handles ownership and lifetime of the
// ChromeSpeechRecognitionPreferences instances (through the inner Service
// class), allowing them to be used in a detached mode (no persistent storage)
// even when a Profile is not available (or has been shutdown).

class ChromeSpeechRecognitionPreferences
    : public content::SpeechRecognitionPreferences,
      public content::NotificationObserver {
 public:
  static void InitializeFactory();
  static scoped_refptr<ChromeSpeechRecognitionPreferences> GetForProfile(
      Profile* profile);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;


  // content::SpeechRecognitionPreferences implementation.
  // Called by both Content (on IO thread) and Chrome (on UI thread).
  virtual bool FilterProfanities() const OVERRIDE;

  // Called only by Chrome (on UI thread).
  void SetFilterProfanities(bool filter_profanities);
  void ToggleFilterProfanities();
  bool ShouldShowSecurityNotification(const std::string& context_name) const;
  void SetHasShownSecurityNotification(const std::string& context_name);

 private:
  // The two classes below are needed to handle storage of speech recognition
  // preferences in profile preferences, according to the Chromium Profile
  // Architecture document entitled "The New Way: ProfileKeyedServiceFactory".

  // Singleton that manages instantiation of ChromeSpeechRecognitionPreferences
  // handling its association with Profiles.
  class Factory : public ProfileKeyedServiceFactory {
   public:
    static Factory* GetInstance();
    scoped_refptr<ChromeSpeechRecognitionPreferences> GetForProfile(
        Profile* profile);

   private:
    friend struct DefaultSingletonTraits<Factory>;

    Factory();
    virtual ~Factory();

    // ProfileKeyedServiceFactory methods:
    virtual ProfileKeyedService* BuildServiceInstanceFor(Profile* profile)
        const OVERRIDE;
    virtual void RegisterUserPrefs(PrefService* prefs) OVERRIDE;
    virtual bool ServiceRedirectedInIncognito() const OVERRIDE;
    virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
    virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;

    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  // This wrapper handles the binding between ChromeSpeechRecognitionPreferences
  // instances (which can have a longer lifetime, since they are refcounted) and
  // ProfileKeyedService (which lifetime depends on the Profile service). Upon
  // profile shutdown, the ChromeSpeechRecognitionPreferences instance is
  // detached from profile, meaning that after that point its clients can still
  // use it, but preferences will no longer be kept in sync with the profile.
  class Service : public ProfileKeyedService {
   public:
    explicit Service(Profile* profile);
    virtual ~Service();

    // ProfileKeyedService implementation.
    virtual void Shutdown() OVERRIDE;

    scoped_refptr<ChromeSpeechRecognitionPreferences> GetPreferences() const;

   private:
    scoped_refptr<ChromeSpeechRecognitionPreferences> preferences_;

    DISALLOW_COPY_AND_ASSIGN(Service);
  };

  explicit ChromeSpeechRecognitionPreferences(Profile* profile);
  virtual ~ChromeSpeechRecognitionPreferences();

  void DetachFromProfile();
  void ReloadPreference(const std::string& key);

  Profile* profile_;  // NULL when detached.
  scoped_ptr<PrefChangeRegistrar> pref_change_registrar_;
  bool filter_profanities_;
  scoped_ptr<base::ListValue> notifications_shown_;

  // Lock used to ensure exclusive access to preference variables that are
  // accessed by both threads (note: mutable is required to keep getters const).
  mutable base::Lock preferences_lock_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSpeechRecognitionPreferences);
};

#endif  // CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_PREFERENCES_H_
