// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides access to the Contextual Search preference that can be called on any
// platform, and provides an entry point for Unified Consent to update the
// Contextual Search enabled preference with associate notification.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_CONTEXTUAL_SEARCH_PREFERENCE_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_CONTEXTUAL_SEARCH_PREFERENCE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/no_destructor.h"

class PrefService;

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.contextualsearch
enum ContextualSearchPreviousPreferenceMetadata {
  UNKNOWN = 0,
  WAS_UNDECIDED = 1,
  WAS_DECIDED = 2,
};

namespace contextual_search {

// Returns the name of the Contextual Search preference, used to check if the
// feature is enabled.
const char* GetPrefName();

// Returns whether Contextual Search is enabled.
bool IsEnabled(const PrefService& prefs);

// Holds metadata about a preference-changed made by Unified Consent.
class ContextualSearchPreference {
 public:
  // No public constructor, use |GetInstance|.
  ~ContextualSearchPreference() {}

  // Returns the singleton instance of this class, created when needed.
  static ContextualSearchPreference* GetInstance();

  // Fully-enables or disables Contextual Search through the given
  // |pref_service|.  The previous state of the preference is ignored.
  void SetPref(PrefService* pref_service, bool enable);

  // Enables Contextual Search for users that are currently undecided about the
  // feature.  Uses the given |pref_service| to make the change.
  void EnableIfUndecided(PrefService* pref_service);

  // Gets the previous preference setting's metadata.
  ContextualSearchPreviousPreferenceMetadata GetPreviousPreferenceMetadata();

 private:
  // No public constructor, use |GetInstance|.
  ContextualSearchPreference();

  // Stores metadata describing what we know about the value of the previous
  // preference.
  ContextualSearchPreviousPreferenceMetadata previous_preference_metadata_;

  // Allows access to our private constructor.
  friend class base::NoDestructor<ContextualSearchPreference>;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchPreference);
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_CONTEXTUAL_SEARCH_PREFERENCE_H_
