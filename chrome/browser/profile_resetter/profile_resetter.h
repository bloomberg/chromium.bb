// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESETTER_H_
#define CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESETTER_H_

#include "base/callback.h"
#include "base/compiler_specific.h"

class Profile;

// This class allows resetting certain aspects of a profile to default values.
// It is used in case the profile has been damaged due to malware or bad user
// settings.
class ProfileResetter {
 public:
  // Flags indicating what aspects of a profile shall be reset.
  enum Resettable {
    DEFAULT_SEARCH_ENGINE = 1 << 0,
    HOMEPAGE = 1 << 1,
    CONTENT_SETTINGS = 1 << 2,
    COOKIES_AND_SITE_DATA = 1 << 3,
    EXTENSIONS = 1 << 4,
    // Update ALL if you add new values and check whether the type of
    // ResettableFlags needs to be enlarged.
    ALL = DEFAULT_SEARCH_ENGINE | HOMEPAGE | CONTENT_SETTINGS |
          COOKIES_AND_SITE_DATA | EXTENSIONS
  };

  // How to handle extensions that shall be reset.
  enum ExtensionHandling {
    DISABLE_EXTENSIONS,
    UNINSTALL_EXTENSIONS
  };

  // Bit vector for Resettable enum.
  typedef uint32 ResettableFlags;

  explicit ProfileResetter(Profile* profile);
  ~ProfileResetter();

  // Resets |resettable_flags| and calls |callback| on the UI thread on
  // completion. If |resettable_flags| contains EXTENSIONS, these are handled
  // according to |extension_handling|.
  void Reset(ResettableFlags resettable_flags,
             ExtensionHandling extension_handling,
             const base::Closure& callback);

 private:
  // Marks |resettable| as done and triggers |callback_| if all pending jobs
  // have completed.
  void MarkAsDone(Resettable resettable);

  void ResetDefaultSearchEngine();
  void ResetHomepage();
  void ResetContentSettings();
  void ResetCookiesAndSiteData();
  void ResetExtensions(ExtensionHandling extension_handling);

  Profile* profile_;

  // Flags of a Resetable indicating which reset operations we are still waiting
  // for.
  ResettableFlags pending_reset_flags_;

  // Called on UI thread when reset has been completed.
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(ProfileResetter);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESETTER_H_
