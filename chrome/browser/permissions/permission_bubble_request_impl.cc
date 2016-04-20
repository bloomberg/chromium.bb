// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_bubble_request_impl.h"

#include "build/build_config.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "grit/theme_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icons_public.h"

PermissionBubbleRequestImpl::PermissionBubbleRequestImpl(
    const GURL& request_origin,
    content::PermissionType permission_type,
    const PermissionDecidedCallback& permission_decided_callback,
    const base::Closure delete_callback)
    : request_origin_(request_origin),
      permission_type_(permission_type),
      permission_decided_callback_(permission_decided_callback),
      delete_callback_(delete_callback),
      is_finished_(false),
      action_taken_(false) {}

PermissionBubbleRequestImpl::~PermissionBubbleRequestImpl() {
  DCHECK(is_finished_);
  if (!action_taken_)
    PermissionUmaUtil::PermissionIgnored(permission_type_, request_origin_);
}

gfx::VectorIconId PermissionBubbleRequestImpl::GetVectorIconId() const {
#if !defined(OS_MACOSX)
  switch (permission_type_) {
    case content::PermissionType::GEOLOCATION:
      return gfx::VectorIconId::LOCATION_ON;
#if defined(ENABLE_NOTIFICATIONS)
    case content::PermissionType::NOTIFICATIONS:
      return gfx::VectorIconId::NOTIFICATIONS;
#endif
#if defined(OS_CHROMEOS)
    // TODO(xhwang): fix this icon, see crrev.com/863263007
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      return gfx::VectorIconId::CHROME_PRODUCT;
#endif
    case content::PermissionType::MIDI_SYSEX:
      return gfx::VectorIconId::MIDI;
    default:
      NOTREACHED();
      return gfx::VectorIconId::VECTOR_ICON_NONE;
  }
#else  // !defined(OS_MACOSX)
  return gfx::VectorIconId::VECTOR_ICON_NONE;
#endif
}

int PermissionBubbleRequestImpl::GetIconId() const {
  int icon_id = IDR_INFOBAR_WARNING;
#if defined(OS_MACOSX)
  switch (permission_type_) {
    case content::PermissionType::GEOLOCATION:
      icon_id = IDR_INFOBAR_GEOLOCATION;
      break;
#if defined(ENABLE_NOTIFICATIONS)
    case content::PermissionType::NOTIFICATIONS:
      icon_id = IDR_INFOBAR_DESKTOP_NOTIFICATIONS;
      break;
#endif
    case content::PermissionType::MIDI_SYSEX:
      icon_id = IDR_ALLOWED_MIDI_SYSEX;
      break;
    default:
      NOTREACHED();
  }
#endif
  return icon_id;
}

base::string16 PermissionBubbleRequestImpl::GetMessageText() const {
  int message_id;
  switch (permission_type_) {
    case content::PermissionType::GEOLOCATION:
      message_id = IDS_GEOLOCATION_INFOBAR_QUESTION;
      break;
#if defined(ENABLE_NOTIFICATIONS)
    case content::PermissionType::NOTIFICATIONS:
      message_id = IDS_NOTIFICATION_PERMISSIONS;
      break;
#endif
    case content::PermissionType::MIDI_SYSEX:
      message_id = IDS_MIDI_SYSEX_INFOBAR_QUESTION;
      break;
    case content::PermissionType::PUSH_MESSAGING:
      message_id = IDS_PUSH_MESSAGES_PERMISSION_QUESTION;
      break;
#if defined(OS_CHROMEOS)
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      message_id = IDS_PROTECTED_MEDIA_IDENTIFIER_INFOBAR_QUESTION;
      break;
#endif
    default:
      NOTREACHED();
      return base::string16();
  }
  return l10n_util::GetStringFUTF16(
      message_id,
      url_formatter::FormatUrlForSecurityDisplay(
          request_origin_, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
}

base::string16 PermissionBubbleRequestImpl::GetMessageTextFragment() const {
  int message_id;
  switch (permission_type_) {
    case content::PermissionType::GEOLOCATION:
      message_id = IDS_GEOLOCATION_INFOBAR_PERMISSION_FRAGMENT;
      break;
#if defined(ENABLE_NOTIFICATIONS)
    case content::PermissionType::NOTIFICATIONS:
      message_id = IDS_NOTIFICATION_PERMISSIONS_FRAGMENT;
      break;
#endif
    case content::PermissionType::MIDI_SYSEX:
      message_id = IDS_MIDI_SYSEX_PERMISSION_FRAGMENT;
      break;
    case content::PermissionType::PUSH_MESSAGING:
      message_id = IDS_PUSH_MESSAGES_BUBBLE_FRAGMENT;
      break;
#if defined(OS_CHROMEOS)
    case content::PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      message_id = IDS_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_FRAGMENT;
      break;
#endif
    default:
      NOTREACHED();
      return base::string16();
  }
  return l10n_util::GetStringUTF16(message_id);
}

GURL PermissionBubbleRequestImpl::GetOrigin() const {
  return request_origin_;
}

void PermissionBubbleRequestImpl::PermissionGranted() {
  RegisterActionTaken();
  permission_decided_callback_.Run(true, CONTENT_SETTING_ALLOW);
}

void PermissionBubbleRequestImpl::PermissionDenied() {
  RegisterActionTaken();
  permission_decided_callback_.Run(true, CONTENT_SETTING_BLOCK);
}

void PermissionBubbleRequestImpl::Cancelled() {
  RegisterActionTaken();
  permission_decided_callback_.Run(false, CONTENT_SETTING_DEFAULT);
}

void PermissionBubbleRequestImpl::RequestFinished() {
  is_finished_ = true;
  delete_callback_.Run();
}
