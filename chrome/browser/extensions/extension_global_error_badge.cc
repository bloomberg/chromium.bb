// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_global_error_badge.h"

#include "base/logging.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

ExtensionGlobalErrorBadge::ExtensionGlobalErrorBadge() {}

ExtensionGlobalErrorBadge::~ExtensionGlobalErrorBadge() {}

bool ExtensionGlobalErrorBadge::HasBadge() {
  return true;
}

bool ExtensionGlobalErrorBadge::HasMenuItem() {
  return true;
}

int ExtensionGlobalErrorBadge::MenuItemCommandID() {
  return GetMenuItemCommandID();
}

string16 ExtensionGlobalErrorBadge::MenuItemLabel() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_WARNINGS_WRENCH_MENU_ITEM);
}

void ExtensionGlobalErrorBadge::ExecuteMenuItem(Browser* browser) {
  ExtensionService* extension_service =
      browser->profile()->GetExtensionService();

  // Suppress all current warnings in the extension service from triggering
  // a badge on the wrench menu in the future of this session.
  extension_service->extension_warnings()->SuppressBadgeForCurrentWarnings();

  browser->ExecuteCommand(IDC_MANAGE_EXTENSIONS);
}

bool ExtensionGlobalErrorBadge::HasBubbleView() {
  return false;
}

string16 ExtensionGlobalErrorBadge::GetBubbleViewTitle() {
  return string16();
}

string16 ExtensionGlobalErrorBadge::GetBubbleViewMessage() {
  return string16();
}

string16 ExtensionGlobalErrorBadge::GetBubbleViewAcceptButtonLabel() {
  return string16();
}

string16 ExtensionGlobalErrorBadge::GetBubbleViewCancelButtonLabel() {
  return string16();
}

void ExtensionGlobalErrorBadge::OnBubbleViewDidClose(Browser* browser) {
}

void ExtensionGlobalErrorBadge::BubbleViewAcceptButtonPressed(
    Browser* browser) {
  NOTREACHED();
}

void ExtensionGlobalErrorBadge::BubbleViewCancelButtonPressed(
    Browser* browser) {
  NOTREACHED();
}

// static
int ExtensionGlobalErrorBadge::GetMenuItemCommandID() {
  return IDC_EXTENSION_ERRORS;
}
