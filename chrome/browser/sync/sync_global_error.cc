// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_global_error.h"

#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/global_error_service.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

typedef GoogleServiceAuthError AuthError;

using namespace sync_ui_util;

SyncGlobalError::SyncGlobalError(ProfileSyncService* service)
    : has_error_(false),
      service_(service) {
  OnStateChanged();
}

SyncGlobalError::~SyncGlobalError() {
}

bool SyncGlobalError::HasBadge() {
#if defined(OS_CHROMEOS)
  // TODO(sail): Due to http://crbug.com/96608 we don't have a wrench menu
  // item on ChromeOS thus we don't badge the wrench menu either.
  return false;
#else
  return GetStatusLabelsForSyncGlobalError(service_, NULL, NULL, NULL) ==
      SYNC_ERROR;
#endif
}

bool SyncGlobalError::HasMenuItem() {
  // TODO(sail): Once http://crbug.com/96608 is fixed this should return
  // true for ChromeOS.
  return false;
}

int SyncGlobalError::MenuItemCommandID() {
  NOTREACHED();
  return 0;
}

string16 SyncGlobalError::MenuItemLabel() {
  string16 label;
  GetStatusLabelsForSyncGlobalError(service_, &label, NULL, NULL);
  return label;
}

void SyncGlobalError::ExecuteMenuItem(Browser* browser) {
  service_->ShowErrorUI();
}

bool SyncGlobalError::HasBubbleView() {
  return GetStatusLabelsForSyncGlobalError(service_, NULL, NULL, NULL) ==
      SYNC_ERROR;
}

string16 SyncGlobalError::GetBubbleViewTitle() {
  return l10n_util::GetStringUTF16(IDS_SYNC_ERROR_BUBBLE_VIEW_TITLE);
}

string16 SyncGlobalError::GetBubbleViewMessage() {
  string16 label;
  GetStatusLabelsForSyncGlobalError(service_, NULL, &label, NULL);
  return label;
}

string16 SyncGlobalError::GetBubbleViewAcceptButtonLabel() {
  string16 label;
  GetStatusLabelsForSyncGlobalError(service_, NULL, NULL, &label);
  return label;
}

string16 SyncGlobalError::GetBubbleViewCancelButtonLabel() {
  return string16();
}

void SyncGlobalError::BubbleViewDidClose() {
}

void SyncGlobalError::BubbleViewAcceptButtonPressed() {
  service_->ShowErrorUI();
}

void SyncGlobalError::BubbleViewCancelButtonPressed() {
  NOTREACHED();
}

void SyncGlobalError::OnStateChanged() {
  bool new_has_error = GetStatusLabelsForSyncGlobalError(
      service_, NULL, NULL, NULL) == SYNC_ERROR;
  if (new_has_error != has_error_) {
    has_error_ = new_has_error;
    GlobalErrorServiceFactory::GetForProfile(
        service_->profile())->NotifyErrorsChanged(this);
  }
}

bool SyncGlobalError::HasCustomizedSyncMenuItem() {
  return GetStatusLabelsForSyncGlobalError(service_, NULL, NULL, NULL) ==
      SYNC_ERROR;
}
