// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_error_ui.h"

#include "base/logging.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::ExtensionIdSet;

ExtensionErrorUI::ExtensionErrorUI(ExtensionService* extension_service)
    : extension_service_(extension_service),
      external_extension_ids_(new ExtensionIdSet),
      blacklisted_extension_ids_(new ExtensionIdSet) {
  DCHECK(extension_service_);
}

ExtensionErrorUI::~ExtensionErrorUI() {
}

void ExtensionErrorUI::AddExternalExtension(const std::string& id) {
  external_extension_ids_->insert(id);
}

void ExtensionErrorUI::AddBlacklistedExtension(const std::string& id) {
  blacklisted_extension_ids_->insert(id);
}

string16 ExtensionErrorUI::GenerateMessageSection(
    const ExtensionIdSet* extensions,
    int extension_template_message_id,
    int app_template_message_id) {
  CHECK(extensions);
  CHECK(extension_template_message_id);
  CHECK(app_template_message_id);
  string16 message;

  for (ExtensionIdSet::const_iterator iter = extensions->begin();
       iter != extensions->end(); ++iter) {
    const extensions::Extension* e =
        extension_service_->GetInstalledExtension(*iter);
    message += l10n_util::GetStringFUTF16(
        e->is_app() ? app_template_message_id : extension_template_message_id,
        UTF8ToUTF16(e->name())) + char16('\n');
  }
  return message;
}

string16 ExtensionErrorUI::GenerateMessage() {
  return GenerateMessageSection(external_extension_ids_.get(),
                                IDS_EXTENSION_ALERT_ITEM_EXTERNAL,
                                IDS_APP_ALERT_ITEM_EXTERNAL) +
         GenerateMessageSection(blacklisted_extension_ids_.get(),
                                IDS_EXTENSION_ALERT_ITEM_BLACKLISTED,
                                IDS_APP_ALERT_ITEM_BLACKLISTED);
}

string16 ExtensionErrorUI::GetBubbleViewMessage() {
  if (message_.empty()) {
    message_ = GenerateMessage();
    if (message_[message_.size()-1] == '\n')
      message_.resize(message_.size()-1);
  }
  return message_;
}

string16 ExtensionErrorUI::GetBubbleViewTitle() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_ALERT_TITLE);
}

string16 ExtensionErrorUI::GetBubbleViewAcceptButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_ALERT_ITEM_OK);
}

string16 ExtensionErrorUI::GetBubbleViewCancelButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_ALERT_ITEM_DETAILS);
}

void ExtensionErrorUI::BubbleViewDidClose() {
  // This call deletes ExtensionErrorUI object referenced by this.
  extension_service_->HandleExtensionAlertClosed();
}

void ExtensionErrorUI::BubbleViewAcceptButtonPressed() {
  extension_service_->HandleExtensionAlertAccept();
}

void ExtensionErrorUI::BubbleViewCancelButtonPressed() {
  extension_service_->HandleExtensionAlertDetails();
}
