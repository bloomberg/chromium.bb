// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WARNING_SET_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WARNING_SET_H_
#pragma once

#include <set>
#include <string>

#include "base/string16.h"

class ExtensionWarning;
class ExtensionGlobalErrorBadge;
class Profile;

// A set of warnings caused by extensions. These warnings (e.g. conflicting
// modifications of network requests by extensions, slow extensions, etc.)
// trigger a warning badge in the UI and and provide means to resolve them.
class ExtensionWarningSet {
 public:
  enum WarningType {
    // Don't use this, it is only intended for the default constructor and
    // does not have localized warning messages for the UI.
    kInvalid = 0,
    // An extension caused excessive network delays.
    kNetworkDelay,
    // This extension failed to modify a network request because the
    // modification conflicted with a modification of another extension.
    kNetworkConflict,
    // The extension repeatedly flushed WebKit's in-memory cache, which slows
    // down the overall performance.
    kRepeatedCacheFlushes,
    kMaxWarningType
  };

  // Returns a localized string describing |warning_type|.
  static string16 GetLocalizedWarning(WarningType warning_type);

  // |profile| may be NULL for testing. In this case, be sure to not insert
  // any warnings.
  explicit ExtensionWarningSet(Profile* profile);
  virtual ~ExtensionWarningSet();

  // Adds a warning and triggers a chrome::NOTIFICATION_EXTENSION_WARNING
  // message if this warning is is new. If the warning is new and has not
  // been suppressed, this may activate a badge on the wrench menu.
  void SetWarning(ExtensionWarningSet::WarningType type,
                  const std::string& extension_id);

  // Clears all warnings of types contained in |types| and triggers a
  // chrome::NOTIFICATION_EXTENSION_WARNING message if such warnings existed.
  // If no warning remains that is not suppressed, this may deactivate a
  // warning badge on the wrench mennu.
  void ClearWarnings(const std::set<WarningType>& types);

  // Suppresses showing a badge for all currently existing warnings in the
  // future.
  void SuppressBadgeForCurrentWarnings();

  // Stores all warnings for extension |extension_id| in |result|. The previous
  // content of |result| is erased.
  void GetWarningsAffectingExtension(
      const std::string& extension_id,
      std::set<WarningType>* result) const;

  // Notifies the ExtensionWarningSet of profile |profile_id| that
  // |extension_ids| caused warning |warning_type|. This function must only be
  // called on the UI thread.
  static void NotifyWarningsOnUI(void* profile_id,
                                 std::set<std::string> extension_ids,
                                 WarningType warning_type);

 protected:
  // Virtual for testing.
  virtual void NotifyWarningsChanged();

 private:
  typedef std::set<ExtensionWarning>::const_iterator const_iterator;
  typedef std::set<ExtensionWarning>::iterator iterator;

  // Shows or hides the warning badge on the wrench menu depending on whether
  // any non-suppressed warnings exist.
  void UpdateWarningBadge();

  // Currently existing warnings.
  std::set<ExtensionWarning> warnings_;

  // Warnings that do not trigger a badge on the wrench menu.
  std::set<ExtensionWarning> badge_suppressions_;

  Profile* profile_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WARNING_SET_H_
