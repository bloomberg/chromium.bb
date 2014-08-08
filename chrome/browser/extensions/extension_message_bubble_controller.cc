// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_message_bubble_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_message_bubble.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// ExtensionMessageBubbleController::Delegate

ExtensionMessageBubbleController::Delegate::Delegate() {
}

ExtensionMessageBubbleController::Delegate::~Delegate() {
}

void ExtensionMessageBubbleController::Delegate::RestrictToSingleExtension(
    const std::string& extension_id) {
  NOTIMPLEMENTED();  // Derived classes that need this should implement.
}

////////////////////////////////////////////////////////////////////////////////
// ExtensionMessageBubbleController

ExtensionMessageBubbleController::ExtensionMessageBubbleController(
    Delegate* delegate, Profile* profile)
    : profile_(profile),
      user_action_(ACTION_BOUNDARY),
      delegate_(delegate),
      initialized_(false) {
}

ExtensionMessageBubbleController::~ExtensionMessageBubbleController() {
}

std::vector<base::string16>
ExtensionMessageBubbleController::GetExtensionList() {
  ExtensionIdList* list = GetOrCreateExtensionList();
  if (list->empty())
    return std::vector<base::string16>();

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  std::vector<base::string16> return_value;
  for (ExtensionIdList::const_iterator it = list->begin();
       it != list->end(); ++it) {
    const Extension* extension =
        registry->GetExtensionById(*it, ExtensionRegistry::EVERYTHING);
    if (extension) {
      return_value.push_back(base::UTF8ToUTF16(extension->name()));
    } else {
      return_value.push_back(
          base::ASCIIToUTF16(std::string("(unknown name) ") + *it));
      // TODO(finnur): Add this as a string to the grd, for next milestone.
    }
  }
  return return_value;
}

const ExtensionIdList& ExtensionMessageBubbleController::GetExtensionIdList() {
  return *GetOrCreateExtensionList();
}

bool ExtensionMessageBubbleController::CloseOnDeactivate() { return false; }

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
  delegate_->OnClose();
}

void ExtensionMessageBubbleController::OnBubbleDismiss() {
  // OnBubbleDismiss() can be called twice when we receive multiple
  // "OnWidgetDestroying" notifications (this can at least happen when we close
  // a window with a notification open). Handle this gracefully.
  if (user_action_ != ACTION_BOUNDARY) {
    DCHECK(user_action_ == ACTION_DISMISS);
    return;
  }

  user_action_ = ACTION_DISMISS;

  delegate_->LogAction(ACTION_DISMISS);
  AcknowledgeExtensions();
  delegate_->OnClose();
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
  delegate_->OnClose();
}

void ExtensionMessageBubbleController::AcknowledgeExtensions() {
  ExtensionIdList* list = GetOrCreateExtensionList();
  for (ExtensionIdList::const_iterator it = list->begin();
       it != list->end(); ++it)
    delegate_->AcknowledgeExtension(*it, user_action_);
}

ExtensionIdList* ExtensionMessageBubbleController::GetOrCreateExtensionList() {
  if (!initialized_) {
    scoped_ptr<const ExtensionSet> extension_set(
        ExtensionRegistry::Get(profile_)->GenerateInstalledExtensionsSet());
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
