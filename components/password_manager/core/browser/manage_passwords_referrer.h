// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MANAGE_PASSWORDS_REFERRER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MANAGE_PASSWORDS_REFERRER_H_

namespace password_manager {

// Enumerates referrers that can trigger a navigation to the manage passwords
// page.
enum class ManagePasswordsReferrer {
  // Corresponds to Chrome's settings page.
  kChromeSettings = 0,
  // Corresponds to the manage passwords bubble when clicking the key icon.
  kManagePasswordsBubble = 1,
  // Corresponds to the context menu following a right click into a password
  // field.
  kPasswordContextMenu = 2,
  // Corresponds to the password dropdown shown when clicking into a password
  // field.
  kPasswordDropdown = 3,
  // Corresponds to the bubble shown when clicking the key icon after a password
  // was generated.
  kPasswordGenerationConfirmation = 4,
  // Corresponds to the profile chooser next to the omnibar ("Autofill Home").
  kProfileChooser = 5,
  kMaxValue = kProfileChooser,
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MANAGE_PASSWORDS_REFERRER_H_
