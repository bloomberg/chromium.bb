// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/midi_permission_infobar_delegate.h"

#include "chrome/browser/content_settings/permission_queue_controller.h"
#include "chrome/browser/content_settings/permission_request_id.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

// static
infobars::InfoBar* MidiPermissionInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const std::string& display_languages,
    ContentSettingsType type) {
  const content::NavigationEntry* committed_entry =
      infobar_service->web_contents()->GetController().GetLastCommittedEntry();
  return infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new MidiPermissionInfoBarDelegate(
          controller, id, requesting_frame,
          committed_entry ? committed_entry->GetUniqueID() : 0,
          display_languages, type))));
}

MidiPermissionInfoBarDelegate::MidiPermissionInfoBarDelegate(
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    int contents_unique_id,
    const std::string& display_languages,
    ContentSettingsType type)
    : PermissionInfobarDelegate(controller, id, requesting_frame, type),
      requesting_frame_(requesting_frame),
      display_languages_(display_languages) {
}

MidiPermissionInfoBarDelegate::~MidiPermissionInfoBarDelegate() {
}

int MidiPermissionInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_MIDI;
}

base::string16 MidiPermissionInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_MIDI_SYSEX_INFOBAR_QUESTION,
      net::FormatUrl(requesting_frame_.GetOrigin(), display_languages_,
                     net::kFormatUrlOmitUsernamePassword |
                     net::kFormatUrlOmitTrailingSlashOnBareHostname,
                     net::UnescapeRule::SPACES, NULL, NULL, NULL));
}
