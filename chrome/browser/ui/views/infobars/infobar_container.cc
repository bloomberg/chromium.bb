// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_container.h"

#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/infobar.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"

InfoBarContainer::Delegate::~Delegate() {
}

InfoBarContainer::InfoBarContainer(Delegate* delegate)
    : delegate_(delegate),
      tab_contents_(NULL) {
}

InfoBarContainer::~InfoBarContainer() {
  // RemoveAllInfoBarsForDestruction() should have already cleared our infobars.
  DCHECK(infobars_.empty());
}

void InfoBarContainer::ChangeTabContents(TabContents* contents) {
  registrar_.RemoveAll();

  while (!infobars_.empty()) {
    InfoBar* infobar = *infobars_.begin();
    // NULL the container pointer first so that if the infobar is currently
    // animating, OnInfoBarAnimated() won't get called; we'll manually trigger
    // this once for the whole set of changes below.  This also prevents
    // InfoBar::MaybeDelete() from calling RemoveInfoBar() a second time if the
    // infobar happens to be at zero height (dunno if this can actually happen).
    infobar->set_container(NULL);
    RemoveInfoBar(infobar);
  }

  tab_contents_ = contents;
  if (tab_contents_) {
    Source<TabContents> tc_source(tab_contents_);
    registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_ADDED,
                   tc_source);
    registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
                   tc_source);
    registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_REPLACED,
                   tc_source);

    for (size_t i = 0; i < tab_contents_->infobar_count(); ++i) {
      // As when we removed the infobars above, we prevent callbacks to
      // OnInfoBarAnimated() for each infobar.
      AddInfoBar(tab_contents_->GetInfoBarDelegateAt(i)->CreateInfoBar(), false,
                 NO_CALLBACK);
    }
  }

  // Now that everything is up to date, signal the delegate to re-layout.
  OnInfoBarHeightChanged(true);
}

void InfoBarContainer::OnInfoBarHeightChanged(bool is_animating) {
  if (delegate_)
    delegate_->InfoBarContainerHeightChanged(is_animating);
}

void InfoBarContainer::RemoveDelegate(InfoBarDelegate* delegate) {
  tab_contents_->RemoveInfoBar(delegate);
}

void InfoBarContainer::RemoveInfoBar(InfoBar* infobar) {
  PlatformSpecificRemoveInfoBar(infobar);
  infobars_.erase(infobar);
}

void InfoBarContainer::RemoveAllInfoBarsForDestruction() {
  // Before we remove any children, we reset |delegate_|, so that no removals
  // will result in us trying to call
  // delegate_->InfoBarContainerHeightChanged().  This is important because at
  // this point |delegate_| may be shutting down, and it's at best unimportant
  // and at worst disastrous to call that.
  delegate_ = NULL;
  ChangeTabContents(NULL);
}

void InfoBarContainer::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::TAB_CONTENTS_INFOBAR_ADDED:
      AddInfoBar(Details<InfoBarDelegate>(details)->CreateInfoBar(), true,
                 WANT_CALLBACK);
      break;

    case NotificationType::TAB_CONTENTS_INFOBAR_REMOVED:
      RemoveInfoBar(Details<InfoBarDelegate>(details).ptr(), true);
      break;

    case NotificationType::TAB_CONTENTS_INFOBAR_REPLACED: {
      typedef std::pair<InfoBarDelegate*, InfoBarDelegate*> InfoBarPair;
      InfoBarPair* infobar_pair = Details<InfoBarPair>(details).ptr();
      RemoveInfoBar(infobar_pair->first, false);
      AddInfoBar(infobar_pair->second->CreateInfoBar(), false, WANT_CALLBACK);
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

void InfoBarContainer::RemoveInfoBar(InfoBarDelegate* delegate,
                                     bool use_animation) {
  // Search for the infobar associated with |delegate|.  We cannot search for
  // |delegate| in |tab_contents_|, because an InfoBar remains alive until its
  // close animation completes, while the delegate is removed from the tab
  // immediately.
  for (InfoBars::iterator i(infobars_.begin()); i != infobars_.end(); ++i) {
    InfoBar* infobar = *i;
    if (infobar->delegate() == delegate) {
      // We merely need hide the infobar; it will call back to RemoveInfoBar()
      // itself once it's hidden.
      infobar->Hide(use_animation);
      break;
    }
  }
}

void InfoBarContainer::AddInfoBar(InfoBar* infobar,
                                  bool animate,
                                  CallbackStatus callback_status) {
  infobars_.insert(infobar);
  PlatformSpecificAddInfoBar(infobar);
  if (callback_status == WANT_CALLBACK)
    infobar->set_container(this);
  infobar->Show(animate);
  if (callback_status == NO_CALLBACK)
    infobar->set_container(this);
}
