// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_service.h"

#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_delegate.h"
#include "chrome/browser/infobars/insecure_content_infobar_delegate.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"


DEFINE_WEB_CONTENTS_USER_DATA_KEY(InfoBarService);

InfoBarDelegate* InfoBarService::AddInfoBar(
    scoped_ptr<InfoBarDelegate> delegate) {
  if (!infobars_enabled_)
    return NULL;

  for (InfoBars::const_iterator i(infobars_.begin()); i != infobars_.end();
       ++i) {
    if ((*i)->EqualsDelegate(delegate.get())) {
      DCHECK_NE(*i, delegate.get());
      return NULL;
    }
  }

  // TODO(pkasting): Consider removing InfoBarService arg from delegate
  // constructors and instead using a setter from here.
  InfoBarDelegate* delegate_ptr = delegate.release();
  infobars_.push_back(delegate_ptr);
  // Add ourselves as an observer for navigations the first time a delegate is
  // added. We use this notification to expire InfoBars that need to expire on
  // page transitions.
  if (infobars_.size() == 1) {
    registrar_.Add(
        this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<content::NavigationController>(
            &web_contents()->GetController()));
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
      content::Source<InfoBarService>(this),
      content::Details<InfoBarAddedDetails>(delegate_ptr));
  return delegate_ptr;
}

void InfoBarService::RemoveInfoBar(InfoBarDelegate* delegate) {
  RemoveInfoBarInternal(delegate, true);
}

InfoBarDelegate* InfoBarService::ReplaceInfoBar(
    InfoBarDelegate* old_delegate,
    scoped_ptr<InfoBarDelegate> new_delegate) {
  if (!infobars_enabled_)
    return AddInfoBar(new_delegate.Pass());  // Deletes the delegate.

  InfoBars::iterator i(std::find(infobars_.begin(), infobars_.end(),
                                 old_delegate));
  DCHECK(i != infobars_.end());

  InfoBarDelegate* new_delegate_ptr = new_delegate.release();
  i = infobars_.insert(i, new_delegate_ptr);
  InfoBarReplacedDetails replaced_details(old_delegate, new_delegate_ptr);
  // Remove the old delegate before notifying, so that if any observers call
  // back to AddInfoBar() or similar, we don't dupe-check against this delegate.
  infobars_.erase(++i);

  old_delegate->clear_owner();
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REPLACED,
      content::Source<InfoBarService>(this),
      content::Details<InfoBarReplacedDetails>(&replaced_details));
  return new_delegate_ptr;
}

InfoBarService::InfoBarService(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      infobars_enabled_(true) {
  DCHECK(web_contents);
  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<content::WebContents>(web_contents));
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

void InfoBarService::RenderViewGone(base::TerminationStatus status) {
  RemoveAllInfoBars(true);
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

void InfoBarService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED) {
    DCHECK(&web_contents()->GetController() ==
           content::Source<content::NavigationController>(source).ptr());

    content::LoadCommittedDetails& committed_details =
        *(content::Details<content::LoadCommittedDetails>(details).ptr());

    // NOTE: It is not safe to change the following code to count upwards or
    // use iterators, as the RemoveInfoBar() call synchronously modifies our
    // delegate list.
    for (size_t i = infobars_.size(); i > 0; --i) {
      InfoBarDelegate* delegate = infobars_[i - 1];
      if (delegate->ShouldExpire(committed_details))
        RemoveInfoBar(delegate);
    }

    return;
  }

  DCHECK_EQ(type, content::NOTIFICATION_WEB_CONTENTS_DESTROYED);
  // The WebContents is going away; be aggressively paranoid and delete
  // ourselves lest other parts of the system attempt to add infobars or use
  // us otherwise during the destruction.
  DCHECK_EQ(web_contents(),
            content::Source<content::WebContents>(source).ptr());
  web_contents()->RemoveUserData(UserDataKey());
  // That was the equivalent of "delete this". This object is now destroyed;
  // returning from this function is the only safe thing to do.
  return;
}

void InfoBarService::RemoveInfoBarInternal(InfoBarDelegate* delegate,
                                           bool animate) {
  if (!infobars_enabled_) {
    DCHECK(infobars_.empty());
    return;
  }

  InfoBars::iterator i(std::find(infobars_.begin(), infobars_.end(), delegate));
  DCHECK(i != infobars_.end());

  delegate->clear_owner();
  // Remove the delegate before notifying, so that if any observers call back to
  // AddInfoBar() or similar, we don't dupe-check against this delegate.
  infobars_.erase(i);
  // Remove ourselves as an observer if we are tracking no more InfoBars.
  if (infobars_.empty()) {
    registrar_.Remove(
        this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::Source<content::NavigationController>(
            &web_contents()->GetController()));
  }

  InfoBarRemovedDetails removed_details(delegate, animate);
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
