// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/push_messaging_infobar_delegate.h"

#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace gcm {

// static
infobars::InfoBar* PushMessagingInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const std::string& display_languages,
    ContentSettingsType type) {
  DCHECK(infobar_service);
  DCHECK(controller);
  return infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new PushMessagingInfoBarDelegate(
          controller, id, requesting_frame, display_languages, type))));
}

PushMessagingInfoBarDelegate::PushMessagingInfoBarDelegate(
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const std::string& display_languages,
    ContentSettingsType type)
  : PermissionInfobarDelegate(controller, id, requesting_frame, type),
    requesting_origin_(requesting_frame.GetOrigin()),
    display_languages_(display_languages) {
}

PushMessagingInfoBarDelegate::~PushMessagingInfoBarDelegate() {
}

base::string16 PushMessagingInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
        IDS_PUSH_MESSAGES_PERMISSION_QUESTION,
        net::FormatUrl(requesting_origin_, display_languages_,
                       net::kFormatUrlOmitUsernamePassword |
                       net::kFormatUrlOmitTrailingSlashOnBareHostname,
                       net::UnescapeRule::SPACES, NULL, NULL, NULL));
}

int PushMessagingInfoBarDelegate::GetIconID() const {
  // TODO(miguelg): change once we have an icon
  return IDR_INFOBAR_WARNING;
}

}  // namespace gcm
