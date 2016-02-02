// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/external_protocol_dialog_delegate.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"

ExternalProtocolDialogDelegate::ExternalProtocolDialogDelegate(
    const GURL& url,
    int render_process_host_id,
    int tab_contents_id)
    : ProtocolDialogDelegate(url),
      render_process_host_id_(render_process_host_id),
      tab_contents_id_(tab_contents_id),
      program_name_(ShellIntegration::GetApplicationNameForProtocol(url)) {
}

ExternalProtocolDialogDelegate::~ExternalProtocolDialogDelegate() {
}

base::string16 ExternalProtocolDialogDelegate::GetMessageText() const {
  const size_t kMaxUrlWithoutSchemeSize = 256;
  const size_t kMaxCommandSize = 256;
  base::string16 elided_url_without_scheme;
  base::string16 elided_command;
  gfx::ElideString(base::ASCIIToUTF16(url().possibly_invalid_spec()),
                  kMaxUrlWithoutSchemeSize, &elided_url_without_scheme);
  gfx::ElideString(program_name_, kMaxCommandSize, &elided_command);

  base::string16 message_text = l10n_util::GetStringFUTF16(
      IDS_EXTERNAL_PROTOCOL_INFORMATION,
      base::ASCIIToUTF16(url().scheme() + ":"),
      elided_url_without_scheme) + base::ASCIIToUTF16("\n\n");

  message_text += l10n_util::GetStringFUTF16(
      IDS_EXTERNAL_PROTOCOL_APPLICATION_TO_LAUNCH,
      elided_command) + base::ASCIIToUTF16("\n\n");

  message_text += l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_WARNING);
  return message_text;
}

base::string16 ExternalProtocolDialogDelegate::GetCheckboxText() const {
  return l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_CHECKBOX_TEXT);
}

base::string16 ExternalProtocolDialogDelegate::GetTitleText() const {
  return l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_TITLE);
}

void ExternalProtocolDialogDelegate::DoAccept(
    const GURL& url,
    bool dont_block) const {
  if (dont_block) {
      ExternalProtocolHandler::SetBlockState(
          url.scheme(), ExternalProtocolHandler::DONT_BLOCK);
  }

  ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(
      url, render_process_host_id_, tab_contents_id_);
}

void ExternalProtocolDialogDelegate::DoCancel(
    const GURL& url,
    bool dont_block) const {
  if (dont_block) {
      ExternalProtocolHandler::SetBlockState(
          url.scheme(), ExternalProtocolHandler::BLOCK);
  }
}
