// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_container.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "grit/generated_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"

InfoBarContainer::Delegate::~Delegate() {
}

InfoBarContainer::InfoBarContainer(Delegate* delegate)
    : delegate_(delegate),
      tab_contents_(NULL) {
  SetID(VIEW_ID_INFO_BAR_CONTAINER);
}

InfoBarContainer::~InfoBarContainer() {
  // Before we remove any children, we reset |delegate_|, so that no removals
  // will result in us trying to call delegate_->InfoBarContainerSizeChanged().
  // This is important because at this point |delegate_| may be shutting down,
  // and it's at best unimportant and at worst disastrous to call that.
  delegate_ = NULL;
  ChangeTabContents(NULL);
}

int InfoBarContainer::VerticalOverlap() {
  return GetVerticalOverlap(NULL);
}

void InfoBarContainer::ChangeTabContents(TabContents* contents) {
  registrar_.RemoveAll();

  while (!infobars_.empty()) {
    InfoBarView* infobar = *infobars_.begin();
    // NULL the container pointer first so OnInfoBarAnimated() won't get called;
    // we'll manually trigger this once for the whole set of changes below.
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
  OnInfoBarAnimated(true);
}

void InfoBarContainer::OnInfoBarAnimated(bool done) {
  if (delegate_)
    delegate_->InfoBarContainerSizeChanged(!done);
}

void InfoBarContainer::RemoveDelegate(InfoBarDelegate* delegate) {
  tab_contents_->RemoveInfoBar(delegate);
}

void InfoBarContainer::RemoveInfoBar(InfoBarView* infobar) {
  RemoveChildView(infobar);
  infobars_.erase(infobar);
}

gfx::Size InfoBarContainer::GetPreferredSize() {
  // We do not have a preferred width (we will expand to fit the available width
  // of the delegate).
  int total_height;
  GetVerticalOverlap(&total_height);
  return gfx::Size(0, total_height);
}

void InfoBarContainer::Layout() {
  int top = GetVerticalOverlap(NULL);

  for (int i = 0; i < child_count(); ++i) {
    View* child = GetChildViewAt(i);
    gfx::Size ps = child->GetPreferredSize();
    top -= static_cast<InfoBarView*>(child)->AnimatedTabHeight();
    child->SetBounds(0, top, width(), ps.height());
    top += ps.height();
  }
}

void InfoBarContainer::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_GROUPING;
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_INFOBAR_CONTAINER);
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

int InfoBarContainer::GetVerticalOverlap(int* total_height) {
  // Our |total_height| is the sum of the preferred heights of the InfoBars
  // contained within us plus the |vertical_overlap|.
  int vertical_overlap = 0;
  int next_child_y = 0;

  for (int i = 0; i < child_count(); ++i) {
    View* child = GetChildViewAt(i);
    gfx::Size ps = child->GetPreferredSize();
    next_child_y -= static_cast<InfoBarView*>(child)->AnimatedTabHeight();
    vertical_overlap = std::max(vertical_overlap, -next_child_y);
    next_child_y += child->GetPreferredSize().height();
  }

  if (total_height)
    *total_height = next_child_y + vertical_overlap;
  return vertical_overlap;
}

void InfoBarContainer::RemoveInfoBar(InfoBarDelegate* delegate,
                                     bool use_animation) {
  // Search for the infobar associated with |delegate|.  We cannot search for
  // |delegate| in |tab_contents_|, because an InfoBar remains alive until its
  // close animation completes, while the delegate is removed from the tab
  // immediately.
  for (InfoBars::iterator i(infobars_.begin()); i != infobars_.end(); ++i) {
    InfoBarView* infobar = *i;
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
  InfoBarView* infobar_view = static_cast<InfoBarView*>(infobar);
  infobars_.insert(infobar_view);
  AddChildView(infobar_view);
  if (callback_status == WANT_CALLBACK)
    infobar_view->set_container(this);
  infobar_view->Show(animate);
  if (callback_status == NO_CALLBACK)
    infobar_view->set_container(this);
}
