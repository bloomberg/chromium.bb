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
  kChromeSettings,
  // Corresponds to the manage passwords bubble when clicking the key icon.
  kManagePasswordsBubble,
  // Corresponds to the context menu following a right click into a password
  // field.
  kPasswordContextMenu,
  // Corresponds to the password dropdown shown when clicking into a password
  // field.
  kPasswordDropdown,
  // Corresponds to the bubble shown when clicking the key icon after a password
  // was generated.
  kPasswordSaveConfirmation,
  // Corresponds to the profile chooser next to the omnibar ("Autofill Home").
  kProfileChooser,
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MANAGE_PASSWORDS_REFERRER_H_
