// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESETTER_H_
#define CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESETTER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/search_engines/template_url_service.h"

class Profile;

// This class allows resetting certain aspects of a profile to default values.
// It is used in case the profile has been damaged due to malware or bad user
// settings.
class ProfileResetter : public base::NonThreadSafe,
                        public BrowsingDataRemover::Observer {
 public:
  // Flags indicating what aspects of a profile shall be reset.
  enum Resettable {
    DEFAULT_SEARCH_ENGINE = 1 << 0,
    HOMEPAGE = 1 << 1,
    CONTENT_SETTINGS = 1 << 2,
    COOKIES_AND_SITE_DATA = 1 << 3,
    EXTENSIONS = 1 << 4,
    STARTUP_PAGES = 1 << 5,
    PINNED_TABS = 1 << 6,
    // Update ALL if you add new values and check whether the type of
    // ResettableFlags needs to be enlarged.
    ALL = DEFAULT_SEARCH_ENGINE | HOMEPAGE | CONTENT_SETTINGS |
          COOKIES_AND_SITE_DATA | EXTENSIONS | STARTUP_PAGES | PINNED_TABS
  };

  // Bit vector for Resettable enum.
  typedef uint32 ResettableFlags;

  COMPILE_ASSERT(sizeof(ResettableFlags) == sizeof(Resettable),
                 type_ResettableFlags_doesnt_match_Resettable);

  explicit ProfileResetter(Profile* profile);
  virtual ~ProfileResetter();

  // Resets |resettable_flags| and calls |callback| on the UI thread on
  // completion. |default_settings| allows the caller to specify some default
  // settings. |default_settings| shouldn't be NULL.
  void Reset(ResettableFlags resettable_flags,
             scoped_ptr<BrandcodedDefaultSettings> master_settings,
             const base::Closure& callback);

  bool IsActive() const;

 private:
  // Marks |resettable| as done and triggers |callback_| if all pending jobs
  // have completed.
  void MarkAsDone(Resettable resettable);

  void ResetDefaultSearchEngine();
  void ResetHomepage();
  void ResetContentSettings();
  void ResetCookiesAndSiteData();
  void ResetExtensions();
  void ResetStartupPages();
  void ResetPinnedTabs();

  // BrowsingDataRemover::Observer:
  virtual void OnBrowsingDataRemoverDone() OVERRIDE;

  // Callback for when TemplateURLService has loaded.
  void OnTemplateURLServiceLoaded();

  Profile* const profile_;
  scoped_ptr<BrandcodedDefaultSettings> master_settings_;
  TemplateURLService* template_url_service_;

  // Flags of a Resetable indicating which reset operations we are still waiting
  // for.
  ResettableFlags pending_reset_flags_;

  // Called on UI thread when reset has been completed.
  base::Closure callback_;

  // If non-null it means removal is in progress. BrowsingDataRemover takes care
  // of deleting itself when done.
  BrowsingDataRemover* cookies_remover_;

  scoped_ptr<TemplateURLService::Subscription> template_url_service_sub_;

  DISALLOW_COPY_AND_ASSIGN(ProfileResetter);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESETTER_H_
