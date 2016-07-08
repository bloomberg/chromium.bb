// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_CATEGORY_STATUS_H_
#define COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_CATEGORY_STATUS_H_

namespace ntp_snippets {

// Represents the status of a category of content suggestions.
// On Android builds, a Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ntp.snippets
enum class ContentSuggestionsCategoryStatus {
  // Content suggestions are available (though the list of available suggestions
  // may be empty simply because there are no reasonable suggestions to be made
  // at the moment).
  AVAILABLE,
  // Content suggestions are provided but not yet loaded.
  AVAILABLE_LOADING,

  // There is no provider that provides suggestions for this category.
  NOT_PROVIDED,
  // The entire content suggestions feature has explicitly been disabled as part
  // of the service configuration.
  ALL_SUGGESTIONS_EXPLICITLY_DISABLED,
  // Content suggestions from a specific category have been disabled as part of
  // the service configuration.
  CATEGORY_EXPLICITLY_DISABLED,

  // Content suggestions are not available because the user is not signed in.
  SIGNED_OUT,
  // Content suggestions are not available because sync is disabled.
  SYNC_DISABLED,
  // Content suggestions are not available because passphrase encryption is
  // enabled (and it should be disabled).
  PASSPHRASE_ENCRYPTION_ENABLED,
  // Content suggestions are not available because history sync is disabled.
  HISTORY_SYNC_DISABLED,
  // Content suggestions are not available because the history sync service is
  // not yet completely initialized and its status is unknown.
  HISTORY_SYNC_STATE_UNKNOWN
};

// Determines whether the given status is one of the AVAILABLE statuses.
bool IsContentSuggestionsCategoryStatusAvailable(
    ContentSuggestionsCategoryStatus status);

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_CONTENT_SUGGESTIONS_CATEGORY_STATUS_H_
