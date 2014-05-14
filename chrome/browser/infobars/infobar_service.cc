// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_service.h"

#include "base/command_line.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/insecure_content_infobar_delegate.h"
#include "chrome/common/render_messages.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(InfoBarService);

using infobars::InfoBar;
using infobars::InfoBarDelegate;
using infobars::InfoBarManager;

// static
InfoBarDelegate::NavigationDetails
    InfoBarService::NavigationDetailsFromLoadCommittedDetails(
        const content::LoadCommittedDetails& details) {
  InfoBarDelegate::NavigationDetails navigation_details;
  navigation_details.entry_id = details.entry->GetUniqueID();
  navigation_details.is_navigation_to_different_page =
      details.is_navigation_to_different_page();
  navigation_details.did_replace_entry = details.did_replace_entry;
  navigation_details.is_main_frame = details.is_main_frame;

  const content::PageTransition transition = details.entry->GetTransitionType();
  navigation_details.is_reload =
      content::PageTransitionStripQualifier(transition) ==
      content::PAGE_TRANSITION_RELOAD;
  navigation_details.is_redirect =
      (transition & content::PAGE_TRANSITION_IS_REDIRECT_MASK) != 0;

  return navigation_details;
}

// static
content::WebContents* InfoBarService::WebContentsFromInfoBar(InfoBar* infobar) {
  if (!infobar || !infobar->owner())
    return NULL;
  InfoBarService* infobar_service =
      static_cast<InfoBarService*>(infobar->owner());
  return infobar_service->web_contents();
}

InfoBarService::InfoBarService(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(web_contents);
}

InfoBarService::~InfoBarService() {
  ShutDown();
}

int InfoBarService::GetActiveEntryID() {
  content::NavigationEntry* active_entry =
      web_contents()->GetController().GetActiveEntry();
  return active_entry ? active_entry->GetUniqueID() : 0;
}

void InfoBarService::NotifyInfoBarAdded(InfoBar* infobar) {
  InfoBarManager::NotifyInfoBarAdded(infobar);
  // TODO(droger): Remove the notifications and have listeners change to be
  // InfoBarManager::Observers instead. See http://crbug.com/354380
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
      content::Source<InfoBarService>(this),
      content::Details<InfoBar::AddedDetails>(infobar));
}

void InfoBarService::NotifyInfoBarRemoved(InfoBar* infobar, bool animate) {
  InfoBarManager::NotifyInfoBarRemoved(infobar, animate);
  // TODO(droger): Remove the notifications and have listeners change to be
  // InfoBarManager::Observers instead. See http://crbug.com/354380
  InfoBar::RemovedDetails removed_details(infobar, animate);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
      content::Source<InfoBarService>(this),
      content::Details<InfoBar::RemovedDetails>(&removed_details));
}

void InfoBarService::RenderProcessGone(base::TerminationStatus status) {
  RemoveAllInfoBars(true);
}

void InfoBarService::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  OnNavigation(NavigationDetailsFromLoadCommittedDetails(load_details));
}

void InfoBarService::WebContentsDestroyed() {
  // The WebContents is going away; be aggressively paranoid and delete
  // ourselves lest other parts of the system attempt to add infobars or use
  // us otherwise during the destruction.
  web_contents()->RemoveUserData(UserDataKey());
  // That was the equivalent of "delete this". This object is now destroyed;
  // returning from this function is the only safe thing to do.
}

bool InfoBarService::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(InfoBarService, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DidBlockDisplayingInsecureContent,
                        OnDidBlockDisplayingInsecureContent)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DidBlockRunningInsecureContent,
                        OnDidBlockRunningInsecureContent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void InfoBarService::OnDidBlockDisplayingInsecureContent() {
  InsecureContentInfoBarDelegate::Create(
      this, InsecureContentInfoBarDelegate::DISPLAY);
}

void InfoBarService::OnDidBlockRunningInsecureContent() {
  InsecureContentInfoBarDelegate::Create(this,
                                         InsecureContentInfoBarDelegate::RUN);
}
