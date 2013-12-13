// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_message_bubble_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_message_bubble.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// ExtensionMessageBubbleController

ExtensionMessageBubbleController::ExtensionMessageBubbleController(
    Delegate* delegate, Profile* profile)
    : service_(extensions::ExtensionSystem::Get(profile)->extension_service()),
      profile_(profile),
      user_action_(ACTION_BOUNDARY),
      delegate_(delegate),
      initialized_(false),
      has_notified_(false) {
}

ExtensionMessageBubbleController::~ExtensionMessageBubbleController() {
}

bool ExtensionMessageBubbleController::ShouldShow() {
  if (has_notified_)
    return false;

  has_notified_ = true;
  return !GetOrCreateExtensionList()->empty();
}

std::vector<string16>
ExtensionMessageBubbleController::GetExtensionList() {
  ExtensionIdList* list = GetOrCreateExtensionList();
  if (list->empty())
    return std::vector<string16>();

  std::vector<string16> return_value;
  for (ExtensionIdList::const_iterator it = list->begin();
       it != list->end(); ++it) {
    const Extension* extension = service_->GetInstalledExtension(*it);
    if (extension) {
      return_value.push_back(UTF8ToUTF16(extension->name()));
    } else {
      return_value.push_back(
          ASCIIToUTF16(std::string("(unknown name) ") + *it));
      // TODO(finnur): Add this as a string to the grd, for next milestone.
    }
  }
  return return_value;
}

const ExtensionIdList& ExtensionMessageBubbleController::GetExtensionIdList() {
  return *GetOrCreateExtensionList();
}

void ExtensionMessageBubbleController::Show(ExtensionMessageBubble* bubble) {
  // Wire up all the callbacks, to get notified what actions the user took.
  base::Closure dismiss_button_callback =
      base::Bind(&ExtensionMessageBubbleController::OnBubbleDismiss,
      base::Unretained(this));
  base::Closure action_button_callback =
      base::Bind(&ExtensionMessageBubbleController::OnBubbleAction,
      base::Unretained(this));
  base::Closure link_callback =
      base::Bind(&ExtensionMessageBubbleController::OnLinkClicked,
      base::Unretained(this));
  bubble->OnActionButtonClicked(action_button_callback);
  bubble->OnDismissButtonClicked(dismiss_button_callback);
  bubble->OnLinkClicked(link_callback);

  bubble->Show();
}

void ExtensionMessageBubbleController::OnBubbleAction() {
  DCHECK_EQ(ACTION_BOUNDARY, user_action_);
  user_action_ = ACTION_EXECUTE;

  delegate_->LogAction(ACTION_EXECUTE);
  delegate_->PerformAction(*GetOrCreateExtensionList());
  AcknowledgeExtensions();
}

void ExtensionMessageBubbleController::OnBubbleDismiss() {
  DCHECK_EQ(ACTION_BOUNDARY, user_action_);
  user_action_ = ACTION_DISMISS;

  delegate_->LogAction(ACTION_DISMISS);
  AcknowledgeExtensions();
}

void ExtensionMessageBubbleController::OnLinkClicked() {
  DCHECK_EQ(ACTION_BOUNDARY, user_action_);
  user_action_ = ACTION_LEARN_MORE;

  delegate_->LogAction(ACTION_LEARN_MORE);
  Browser* browser =
      chrome::FindBrowserWithProfile(profile_, chrome::GetActiveDesktop());
  if (browser) {
    browser->OpenURL(
        content::OpenURLParams(delegate_->GetLearnMoreUrl(),
                               content::Referrer(),
                               NEW_FOREGROUND_TAB,
                               content::PAGE_TRANSITION_LINK,
                               false));
  }
  AcknowledgeExtensions();
}

void ExtensionMessageBubbleController::AcknowledgeExtensions() {
  ExtensionIdList* list = GetOrCreateExtensionList();
  for (ExtensionIdList::const_iterator it = list->begin();
       it != list->end(); ++it)
    delegate_->AcknowledgeExtension(*it, user_action_);
}

ExtensionIdList* ExtensionMessageBubbleController::GetOrCreateExtensionList() {
  if (!service_)
    return &extension_list_;  // Can occur during testing.

  if (!initialized_) {
    scoped_ptr<const ExtensionSet> extension_set(
        service_->GenerateInstalledExtensionsSet());
    for (ExtensionSet::const_iterator it = extension_set->begin();
         it != extension_set->end(); ++it) {
      std::string id = (*it)->id();
      if (!delegate_->ShouldIncludeExtension(id))
        continue;
      extension_list_.push_back(id);
    }

    delegate_->LogExtensionCount(extension_list_.size());
    initialized_ = true;
  }

  return &extension_list_;
}

}  // namespace extensions
