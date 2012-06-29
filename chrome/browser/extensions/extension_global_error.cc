// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_global_error.h"

#include "base/logging.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::ExtensionIdSet;

ExtensionGlobalError::ExtensionGlobalError(ExtensionService* extension_service)
    : extension_service_(extension_service),
      external_extension_ids_(new ExtensionIdSet),
      blacklisted_extension_ids_(new ExtensionIdSet),
      orphaned_extension_ids_(new ExtensionIdSet) {
  DCHECK(extension_service_);
}

ExtensionGlobalError::~ExtensionGlobalError() {
}

void ExtensionGlobalError::AddExternalExtension(const std::string& id) {
  external_extension_ids_->insert(id);
}

void ExtensionGlobalError::AddBlacklistedExtension(const std::string& id) {
  blacklisted_extension_ids_->insert(id);
}

void ExtensionGlobalError::AddOrphanedExtension(const std::string& id) {
  orphaned_extension_ids_->insert(id);
}

bool ExtensionGlobalError::HasBadge() {
  return false;
}

bool ExtensionGlobalError::HasMenuItem() {
  return false;
}

int ExtensionGlobalError::MenuItemCommandID() {
  NOTREACHED();
  return 0;
}

string16 ExtensionGlobalError::MenuItemLabel() {
  NOTREACHED();
  return NULL;
}

void ExtensionGlobalError::ExecuteMenuItem(Browser* browser) {
  NOTREACHED();
}

bool ExtensionGlobalError::HasBubbleView() {
  return true;
}

string16 ExtensionGlobalError::GetBubbleViewTitle() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_ALERT_TITLE);
}

string16 ExtensionGlobalError::GenerateMessageSection(
    const ExtensionIdSet* extensions,
    int template_message_id) {
  CHECK(extensions);
  CHECK(template_message_id);
  string16 message;

  for (ExtensionIdSet::const_iterator iter = extensions->begin();
       iter != extensions->end(); ++iter) {
    const extensions::Extension* e = extension_service_->GetExtensionById(*iter,
        true);
    message += l10n_util::GetStringFUTF16(template_message_id,
                                          string16(ASCIIToUTF16(e->name())));
  }
  return message;
}

string16 ExtensionGlobalError::GenerateMessage() {
  return GenerateMessageSection(external_extension_ids_.get(),
                                IDS_EXTENSION_ALERT_ITEM_EXTERNAL) +
         GenerateMessageSection(blacklisted_extension_ids_.get(),
                                IDS_EXTENSION_ALERT_ITEM_BLACKLISTED) +
         GenerateMessageSection(orphaned_extension_ids_.get(),
                                IDS_EXTENSION_ALERT_ITEM_ORPHANED);
}

string16 ExtensionGlobalError::GetBubbleViewMessage() {
  if (message_.empty()) {
    message_ = GenerateMessage();
    if (message_[message_.size()-1] == '\n')
      message_.resize(message_.size()-1);
  }
  return message_;
}

string16 ExtensionGlobalError::GetBubbleViewAcceptButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_ALERT_ITEM_OK);
}

string16 ExtensionGlobalError::GetBubbleViewCancelButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_ALERT_ITEM_DETAILS);
}

void ExtensionGlobalError::OnBubbleViewDidClose(Browser* browser) {
  extension_service_->HandleExtensionAlertClosed();
}

void ExtensionGlobalError::BubbleViewAcceptButtonPressed(Browser* browser) {
  extension_service_->HandleExtensionAlertAccept();
}

void ExtensionGlobalError::BubbleViewCancelButtonPressed(Browser* browser) {
  extension_service_->HandleExtensionAlertDetails(browser);
}
