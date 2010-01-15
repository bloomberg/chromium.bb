// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/infobars/infobar_container.h"

#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/infobars/infobars.h"
#include "chrome/common/notification_service.h"

// InfoBarContainer, public: ---------------------------------------------------

InfoBarContainer::InfoBarContainer(BrowserView* browser_view)
    : browser_view_(browser_view),
      tab_contents_(NULL) {
  SetID(VIEW_ID_INFO_BAR_CONTAINER);
}

InfoBarContainer::~InfoBarContainer() {
  // We NULL this pointer before resetting the TabContents to prevent view
  // hierarchy modifications from attempting to adjust the BrowserView, which is
  // in the process of shutting down.
  browser_view_ = NULL;
  ChangeTabContents(NULL);
}

void InfoBarContainer::ChangeTabContents(TabContents* contents) {
  registrar_.RemoveAll();
  // No need to delete the child views here, their removal from the view
  // hierarchy does this automatically (see InfoBar::InfoBarRemoved).
  RemoveAllChildViews(false);
  tab_contents_ = contents;
  if (tab_contents_) {
    UpdateInfoBars();
    Source<TabContents> tc_source(tab_contents_);
    registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_ADDED,
                   tc_source);
    registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
                   tc_source);
    registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_REPLACED,
                   tc_source);
  }
}

void InfoBarContainer::InfoBarAnimated(bool completed) {
  if (browser_view_)
    browser_view_->SelectedTabToolbarSizeChanged(!completed);
}

void InfoBarContainer::RemoveDelegate(InfoBarDelegate* delegate) {
  tab_contents_->RemoveInfoBar(delegate);
}

// InfoBarContainer, views::View overrides: ------------------------------------

gfx::Size InfoBarContainer::GetPreferredSize() {
  // We do not have a preferred width (we will expand to fit the available width
  // of the BrowserView). Our preferred height is the sum of the preferred
  // heights of the InfoBars contained within us.
  int height = 0;
  for (int i = 0; i < GetChildViewCount(); ++i)
    height += GetChildViewAt(i)->GetPreferredSize().height();
  return gfx::Size(0, height);
}

void InfoBarContainer::Layout() {
  int top = 0;
  for (int i = 0; i < GetChildViewCount(); ++i) {
    views::View* child = GetChildViewAt(i);
    gfx::Size ps = child->GetPreferredSize();
    child->SetBounds(0, top, width(), ps.height());
    top += ps.height();
  }
}

bool InfoBarContainer::GetAccessibleName(std::wstring* name) {
  DCHECK(name);

  if (!accessible_name_.empty()) {
    name->assign(accessible_name_);
    return true;
  }
  return false;
}

bool InfoBarContainer::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_TOOLBAR;
  return true;
}

void InfoBarContainer::SetAccessibleName(const std::wstring& name) {
  accessible_name_.assign(name);
}

void InfoBarContainer::ViewHierarchyChanged(bool is_add,
                                            views::View* parent,
                                            views::View* child) {
  if (parent == this && child->GetParent() == this && browser_view_) {
    // An InfoBar child was added or removed. Tell the BrowserView it needs to
    // re-layout since our preferred size will have changed.
    browser_view_->SelectedTabToolbarSizeChanged(false);
  }
}

// InfoBarContainer, NotificationObserver implementation: ----------------------

void InfoBarContainer::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  if (type == NotificationType::TAB_CONTENTS_INFOBAR_ADDED) {
    AddInfoBar(Details<InfoBarDelegate>(details).ptr(), true);  // animated
  } else if (type == NotificationType::TAB_CONTENTS_INFOBAR_REMOVED) {
    RemoveInfoBar(Details<InfoBarDelegate>(details).ptr(), true);  // animated
  } else if (type == NotificationType::TAB_CONTENTS_INFOBAR_REPLACED) {
    std::pair<InfoBarDelegate*, InfoBarDelegate*>* delegates =
        Details<std::pair<InfoBarDelegate*, InfoBarDelegate*> >(details).ptr();
    ReplaceInfoBar(delegates->first, delegates->second);
  } else {
    NOTREACHED();
  }
}

// InfoBarContainer, private: --------------------------------------------------

void InfoBarContainer::UpdateInfoBars() {
  for (int i = 0; i < tab_contents_->infobar_delegate_count(); ++i) {
    InfoBarDelegate* delegate = tab_contents_->GetInfoBarDelegateAt(i);
    InfoBar* infobar = delegate->CreateInfoBar();
    infobar->set_container(this);
    AddChildView(infobar);
    infobar->Open();
  }
}

void InfoBarContainer::AddInfoBar(InfoBarDelegate* delegate,
                                  bool use_animation) {
  InfoBar* infobar = delegate->CreateInfoBar();
  infobar->set_container(this);
  AddChildView(infobar);

  if (use_animation)
    infobar->AnimateOpen();
  else
    infobar->Open();
}

void InfoBarContainer::RemoveInfoBar(InfoBarDelegate* delegate,
                                     bool use_animation) {
  int index = 0;
  for (; index < tab_contents_->infobar_delegate_count(); ++index) {
    if (tab_contents_->GetInfoBarDelegateAt(index) == delegate)
      break;
  }

  InfoBar* infobar = static_cast<InfoBar*>(GetChildViewAt(index));
  if (use_animation) {
    // The View will be removed once the Close animation completes.
    infobar->AnimateClose();
  } else {
    infobar->Close();
  }
}

void InfoBarContainer::ReplaceInfoBar(InfoBarDelegate* old_delegate,
                                      InfoBarDelegate* new_delegate) {
  RemoveInfoBar(old_delegate, false);  // no animation
  AddInfoBar(new_delegate, false);  // no animation
}
