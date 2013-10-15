// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_service.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_delegate.h"
#include "chrome/browser/infobars/insecure_content_infobar_delegate.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"


DEFINE_WEB_CONTENTS_USER_DATA_KEY(InfoBarService);

InfoBarDelegate* InfoBarService::AddInfoBar(
    scoped_ptr<InfoBarDelegate> infobar) {
  DCHECK(infobar);
  if (!infobars_enabled_)
    return NULL;

  for (InfoBars::const_iterator i(infobars_.begin()); i != infobars_.end();
       ++i) {
    if ((*i)->EqualsDelegate(infobar.get())) {
      DCHECK_NE(*i, infobar.get());
      return NULL;
    }
  }

  InfoBarDelegate* infobar_ptr = infobar.release();
  infobars_.push_back(infobar_ptr);
  // TODO(pkasting): Remove InfoBarService arg from delegate constructors and
  // instead use a setter from here.

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
      content::Source<InfoBarService>(this),
      content::Details<InfoBarAddedDetails>(infobar_ptr));
  return infobar_ptr;
}

void InfoBarService::RemoveInfoBar(InfoBarDelegate* infobar) {
  RemoveInfoBarInternal(infobar, true);
}

InfoBarDelegate* InfoBarService::ReplaceInfoBar(
    InfoBarDelegate* old_infobar,
    scoped_ptr<InfoBarDelegate> new_infobar) {
  DCHECK(old_infobar);
  if (!infobars_enabled_)
    return AddInfoBar(new_infobar.Pass());  // Deletes the delegate.
  DCHECK(new_infobar);

  InfoBars::iterator i(std::find(infobars_.begin(), infobars_.end(),
                                 old_infobar));
  DCHECK(i != infobars_.end());

  InfoBarDelegate* new_infobar_ptr = new_infobar.release();
  i = infobars_.insert(i, new_infobar_ptr);
  InfoBarReplacedDetails replaced_details(old_infobar, new_infobar_ptr);

  // Remove the old infobar before notifying, so that if any observers call
  // back to AddInfoBar() or similar, we don't dupe-check against this infobar.
  infobars_.erase(++i);

  old_infobar->clear_owner();
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REPLACED,
      content::Source<InfoBarService>(this),
      content::Details<InfoBarReplacedDetails>(&replaced_details));
  return new_infobar_ptr;
}

InfoBarService::InfoBarService(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      infobars_enabled_(true) {
  DCHECK(web_contents);
}

InfoBarService::~InfoBarService() {
  // Destroy all remaining InfoBars.  It's important to not animate here so that
  // we guarantee that we'll delete all delegates before we do anything else.
  //
  // TODO(pkasting): If there is no InfoBarContainer, this leaks all the
  // InfoBarDelegates.  This will be fixed once we call CloseSoon() directly on
  // Infobars.
  RemoveAllInfoBars(false);
}

void InfoBarService::RenderProcessGone(base::TerminationStatus status) {
  RemoveAllInfoBars(true);
}

void InfoBarService::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  // NOTE: It is not safe to change the following code to count upwards or
  // use iterators, as the RemoveInfoBar() call synchronously modifies our
  // delegate list.
  for (size_t i = infobars_.size(); i > 0; --i) {
    InfoBarDelegate* infobar = infobars_[i - 1];
    if (infobar->ShouldExpire(load_details))
      RemoveInfoBar(infobar);
  }
}

void InfoBarService::WebContentsDestroyed(content::WebContents* web_contents) {
  // The WebContents is going away; be aggressively paranoid and delete
  // ourselves lest other parts of the system attempt to add infobars or use
  // us otherwise during the destruction.
  web_contents->RemoveUserData(UserDataKey());
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

void InfoBarService::RemoveInfoBarInternal(InfoBarDelegate* infobar,
                                           bool animate) {
  DCHECK(infobar);
  if (!infobars_enabled_) {
    DCHECK(infobars_.empty());
    return;
  }

  InfoBars::iterator i(std::find(infobars_.begin(), infobars_.end(), infobar));
  DCHECK(i != infobars_.end());

  infobar->clear_owner();
  // Remove the infobar before notifying, so that if any observers call back to
  // AddInfoBar() or similar, we don't dupe-check against this infobar.
  infobars_.erase(i);

  InfoBarRemovedDetails removed_details(infobar, animate);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
      content::Source<InfoBarService>(this),
      content::Details<InfoBarRemovedDetails>(&removed_details));
}

void InfoBarService::RemoveAllInfoBars(bool animate) {
  while (!infobars_.empty())
    RemoveInfoBarInternal(infobars_.back(), animate);
}

void InfoBarService::OnDidBlockDisplayingInsecureContent() {
  InsecureContentInfoBarDelegate::Create(
      this, InsecureContentInfoBarDelegate::DISPLAY);
}

void InfoBarService::OnDidBlockRunningInsecureContent() {
  InsecureContentInfoBarDelegate::Create(this,
                                         InsecureContentInfoBarDelegate::RUN);
}
