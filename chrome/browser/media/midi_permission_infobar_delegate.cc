// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/midi_permission_infobar_delegate.h"

#include "chrome/browser/content_settings/permission_queue_controller.h"
#include "chrome/browser/content_settings/permission_request_id.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

// static
InfoBarDelegate* MIDIPermissionInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const std::string& display_languages) {
  return infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new MIDIPermissionInfoBarDelegate(infobar_service,
                                        controller,
                                        id,
                                        requesting_frame,
                                        display_languages)));
}

MIDIPermissionInfoBarDelegate::MIDIPermissionInfoBarDelegate(
    InfoBarService* infobar_service,
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const std::string& display_languages)
    : ConfirmInfoBarDelegate(infobar_service),
      controller_(controller),
      id_(id),
      requesting_frame_(requesting_frame),
      display_languages_(display_languages) {
  const content::NavigationEntry* committed_entry = infobar_service->
      web_contents()->GetController().GetLastCommittedEntry();
  contents_unique_id_ = committed_entry ? committed_entry->GetUniqueID() : 0;
}

void MIDIPermissionInfoBarDelegate::InfoBarDismissed() {
  SetPermission(false, false);
}

int MIDIPermissionInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_MIDI_SYSEX;
}

InfoBarDelegate::Type MIDIPermissionInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

bool MIDIPermissionInfoBarDelegate::ShouldExpireInternal(
    const content::LoadCommittedDetails& details) const {
  // This implementation matches InfoBarDelegate::ShouldExpireInternal(), but
  // uses the unique ID we set in the constructor instead of that stored in the
  // base class.
  return (contents_unique_id_ != details.entry->GetUniqueID()) ||
      (content::PageTransitionStripQualifier(
          details.entry->GetTransitionType()) ==
              content::PAGE_TRANSITION_RELOAD);
}

string16 MIDIPermissionInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_MIDI_SYSEX_INFOBAR_QUESTION,
      net::FormatUrl(requesting_frame_.GetOrigin(), display_languages_));
}

string16 MIDIPermissionInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_MIDI_SYSEX_ALLOW_BUTTON : IDS_MIDI_SYSEX_DENY_BUTTON);
}

bool MIDIPermissionInfoBarDelegate::Accept() {
  SetPermission(true, true);
  return true;
}

bool MIDIPermissionInfoBarDelegate::Cancel() {
  SetPermission(true, false);
  return true;
}

void MIDIPermissionInfoBarDelegate::SetPermission(bool update_content_setting,
                                                  bool allowed) {
  controller_->OnPermissionSet(id_, requesting_frame_,
                               web_contents()->GetURL(),
                               update_content_setting, allowed);
}

