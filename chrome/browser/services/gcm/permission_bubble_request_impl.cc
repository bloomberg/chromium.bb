// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/permission_bubble_request_impl.h"

#include "chrome/browser/services/gcm/permission_context_base.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace gcm {

PermissionBubbleRequestImpl::PermissionBubbleRequestImpl(
    const GURL& request_origin,
    bool user_gesture,
    ContentSettingsType type,
    const std::string& display_languages,
    const base::Callback<void(bool, bool)> permission_decided_callback,
    const base::Closure delete_callback)
    : request_origin_(request_origin),
      user_gesture_(user_gesture),
      type_(type),
      display_languages_(display_languages),
      permission_decided_callback_(permission_decided_callback),
      delete_callback_(delete_callback),
      is_finished_(false) {}

PermissionBubbleRequestImpl::~PermissionBubbleRequestImpl() {
  DCHECK(is_finished_);
}

int PermissionBubbleRequestImpl::GetIconID() const {
  switch (type_) {
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
      return IDR_INFOBAR_WARNING;
    default:
      NOTREACHED();
  }
  return IDR_INFOBAR_WARNING;
}

base::string16 PermissionBubbleRequestImpl::GetMessageText() const {
  switch (type_) {
      case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
        return l10n_util::GetStringUTF16(IDS_PUSH_MESSAGES_BUBBLE_TEXT);
      default:
        NOTREACHED();
    }
  return base::string16();
}

base::string16 PermissionBubbleRequestImpl::GetMessageTextFragment() const {
  switch (type_) {
       case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
         return l10n_util::GetStringUTF16(IDS_PUSH_MESSAGES_BUBBLE_FRAGMENT);
       default:
         NOTREACHED();
     }
   return base::string16();
}

bool PermissionBubbleRequestImpl::HasUserGesture() const {
  return user_gesture_;
}

GURL PermissionBubbleRequestImpl::GetRequestingHostname() const {
  return request_origin_;
}

void PermissionBubbleRequestImpl::PermissionGranted() {
  permission_decided_callback_.Run(true, true);
}

void PermissionBubbleRequestImpl::PermissionDenied() {
  permission_decided_callback_.Run(true, false);
}

void PermissionBubbleRequestImpl::Cancelled() {
  permission_decided_callback_.Run(false, false);
}

void PermissionBubbleRequestImpl::RequestFinished() {
  is_finished_ = true;
  delete_callback_.Run();
}

}  // namespace gcm
