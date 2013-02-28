// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_error_ui_default.h"

#include "base/logging.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/host_desktop.h"

ExtensionErrorUIDefault::ExtensionErrorUIDefault(
    ExtensionService* extension_service)
    : ExtensionErrorUI(extension_service),
      browser_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(global_error_(
          new ExtensionGlobalError(this))) {
}

ExtensionErrorUIDefault::~ExtensionErrorUIDefault() {
}

bool ExtensionErrorUIDefault::ShowErrorInBubbleView() {
  Browser* browser =
      chrome::FindLastActiveWithProfile(extension_service()->profile(),
                                        chrome::GetActiveDesktop());
  if (!browser)
    return false;

  browser_ = browser;
  global_error_->ShowBubbleView(browser);
  return true;
}

void ExtensionErrorUIDefault::ShowExtensions() {
  DCHECK(browser_);
  chrome::ShowExtensions(browser_, std::string());
}

ExtensionErrorUIDefault::ExtensionGlobalError::ExtensionGlobalError(
    ExtensionErrorUIDefault* error_ui)
    : error_ui_(error_ui) {
}

bool ExtensionErrorUIDefault::ExtensionGlobalError::HasBadge() {
  return false;
}

bool ExtensionErrorUIDefault::ExtensionGlobalError::HasMenuItem() {
  return false;
}

int ExtensionErrorUIDefault::ExtensionGlobalError::MenuItemCommandID() {
  NOTREACHED();
  return 0;
}

string16 ExtensionErrorUIDefault::ExtensionGlobalError::MenuItemLabel() {
  NOTREACHED();
  return NULL;
}

void ExtensionErrorUIDefault::ExtensionGlobalError::ExecuteMenuItem(
    Browser* browser) {
  NOTREACHED();
}

bool ExtensionErrorUIDefault::ExtensionGlobalError::HasBubbleView() {
  return true;
}

string16 ExtensionErrorUIDefault::ExtensionGlobalError::GetBubbleViewTitle() {
  return error_ui_->GetBubbleViewTitle();
}

string16 ExtensionErrorUIDefault::ExtensionGlobalError::GetBubbleViewMessage() {
  return error_ui_->GetBubbleViewMessage();
}

string16 ExtensionErrorUIDefault::ExtensionGlobalError::
    GetBubbleViewAcceptButtonLabel() {
  return error_ui_->GetBubbleViewAcceptButtonLabel();
}

string16 ExtensionErrorUIDefault::ExtensionGlobalError::
    GetBubbleViewCancelButtonLabel() {
  return error_ui_->GetBubbleViewCancelButtonLabel();
}

void ExtensionErrorUIDefault::ExtensionGlobalError::
    OnBubbleViewDidClose(Browser* browser) {
  // This call deletes error_ui_ (and as a result of error_ui_ destruction,
  // object pointed by this also gets deleted).
  error_ui_->BubbleViewDidClose();
}

void ExtensionErrorUIDefault::ExtensionGlobalError::
    BubbleViewAcceptButtonPressed(Browser* browser) {
  error_ui_->BubbleViewAcceptButtonPressed();
}

void ExtensionErrorUIDefault::ExtensionGlobalError::
    BubbleViewCancelButtonPressed(Browser* browser) {
  error_ui_->BubbleViewCancelButtonPressed();
}

// static
ExtensionErrorUI* ExtensionErrorUI::Create(
    ExtensionService* extension_service) {
  return new ExtensionErrorUIDefault(extension_service);
}
