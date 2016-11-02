// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/blocked_action_bubble_delegate.h"

#include "base/callback_helpers.h"
#include "base/strings/string16.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

BlockedActionBubbleDelegate::BlockedActionBubbleDelegate(
    const base::Callback<void(CloseAction)>& callback,
    const std::string& extension_id)
    : callback_(callback), extension_id_(extension_id) {}

BlockedActionBubbleDelegate::~BlockedActionBubbleDelegate() {}

bool BlockedActionBubbleDelegate::ShouldShow() {
  // TODO(devlin): Technically, this could be wrong if the extension no longer
  // wants to run, was unloaded, etc. We should update this.
  return true;
}

bool BlockedActionBubbleDelegate::ShouldCloseOnDeactivate() {
  return true;
}

base::string16 BlockedActionBubbleDelegate::GetHeadingText() {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_HEADING);
}

base::string16 BlockedActionBubbleDelegate::GetBodyText(
    bool anchored_to_action) {
  return l10n_util::GetStringUTF16(IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_CONTENT);
}

base::string16 BlockedActionBubbleDelegate::GetItemListText() {
  return base::string16();  // No item list.
}

base::string16 BlockedActionBubbleDelegate::GetActionButtonText() {
  return l10n_util::GetStringUTF16(
      IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_OK_BUTTON);
}

base::string16 BlockedActionBubbleDelegate::GetDismissButtonText() {
  return l10n_util::GetStringUTF16(
      IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_CANCEL_BUTTON);
}

base::string16 BlockedActionBubbleDelegate::GetLearnMoreButtonText() {
  return base::string16();  // No learn more link.
}

std::string BlockedActionBubbleDelegate::GetAnchorActionId() {
  return extension_id_;
}

void BlockedActionBubbleDelegate::OnBubbleShown() {}

void BlockedActionBubbleDelegate::OnBubbleClosed(CloseAction action) {
  base::ResetAndReturn(&callback_).Run(action);
}

std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
BlockedActionBubbleDelegate::GetExtraViewInfo() {
  return nullptr;
}
