// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_container.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

InfoBarContainer::Delegate::~Delegate() {
}

InfoBarContainer::InfoBarContainer(Delegate* delegate)
    : delegate_(delegate),
      tab_contents_(NULL) {
  SetID(VIEW_ID_INFO_BAR_CONTAINER);
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_INFOBAR_CONTAINER));
}

InfoBarContainer::~InfoBarContainer() {
  // Before we remove any children, we reset |delegate_|, so that no removals
  // will result in us trying to call delegate_->InfoBarContainerSizeChanged().
  // This is important because at this point |delegate_| may be shutting down,
  // and it's at best unimportant and at worst disastrous to call that.
  delegate_ = NULL;
  ChangeTabContents(NULL);
}

void InfoBarContainer::ChangeTabContents(TabContents* contents) {
  registrar_.RemoveAll();
  // No need to delete the child views here, their removal from the view
  // hierarchy does this automatically (see InfoBarView::InfoBarRemoved).
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

void InfoBarContainer::OnInfoBarAnimated(bool done) {
  if (delegate_)
    delegate_->InfoBarContainerSizeChanged(!done);
}

void InfoBarContainer::RemoveDelegate(InfoBarDelegate* delegate) {
  tab_contents_->RemoveInfoBar(delegate);
}

void InfoBarContainer::PaintInfoBarArrows(gfx::Canvas* canvas,
                                          View* outer_view,
                                          int arrow_center_x) {
  for (int i = 0; i < child_count(); ++i) {
    InfoBarView* infobar = static_cast<InfoBarView*>(GetChildViewAt(i));
    infobar->PaintArrow(canvas, outer_view, arrow_center_x);
  }
}

gfx::Size InfoBarContainer::GetPreferredSize() {
  // We do not have a preferred width (we will expand to fit the available width
  // of the delegate). Our preferred height is the sum of the preferred heights
  // of the InfoBars contained within us.
  int height = 0;
  for (int i = 0; i < child_count(); ++i)
    height += GetChildViewAt(i)->GetPreferredSize().height();
  return gfx::Size(0, height);
}

void InfoBarContainer::Layout() {
  int top = 0;
  for (int i = 0; i < child_count(); ++i) {
    View* child = GetChildViewAt(i);
    gfx::Size ps = child->GetPreferredSize();
    child->SetBounds(0, top, width(), ps.height());
    top += ps.height();
  }
}

AccessibilityTypes::Role InfoBarContainer::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_GROUPING;
}

void InfoBarContainer::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::TAB_CONTENTS_INFOBAR_ADDED:
      AddInfoBar(Details<InfoBarDelegate>(details).ptr(), true);  // animated
      break;

    case NotificationType::TAB_CONTENTS_INFOBAR_REMOVED:
      RemoveInfoBar(Details<InfoBarDelegate>(details).ptr(), true);  // animated
      break;

    case NotificationType::TAB_CONTENTS_INFOBAR_REPLACED: {
      typedef std::pair<InfoBarDelegate*, InfoBarDelegate*> InfoBarPair;
      InfoBarPair* infobar_pair = Details<InfoBarPair>(details).ptr();
      ReplaceInfoBar(infobar_pair->first, infobar_pair->second);
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

void InfoBarContainer::UpdateInfoBars() {
  for (size_t i = 0; i < tab_contents_->infobar_count(); ++i) {
    InfoBarDelegate* delegate = tab_contents_->GetInfoBarDelegateAt(i);
    InfoBarView* infobar = static_cast<InfoBarView*>(delegate->CreateInfoBar());
    infobar->set_container(this);
    AddChildView(infobar);
    infobar->Open();
  }
}

void InfoBarContainer::AddInfoBar(InfoBarDelegate* delegate,
                                  bool use_animation) {
  InfoBarView* infobar = static_cast<InfoBarView*>(delegate->CreateInfoBar());
  infobar->set_container(this);
  AddChildView(infobar);

  if (use_animation)
    infobar->AnimateOpen();
  else
    infobar->Open();
}

void InfoBarContainer::RemoveInfoBar(InfoBarDelegate* delegate,
                                     bool use_animation) {
  // Search for infobar associated with |delegate| among child views.
  // We cannot search for |delegate| in tab_contents, because an infobar remains
  // a child view until its close animation completes, which can result in
  // different number of infobars in container and infobar delegates in tab
  // contents.
  for (int i = 0; i < child_count(); ++i) {
    InfoBarView* infobar = static_cast<InfoBarView*>(GetChildViewAt(i));
    if (infobar->delegate() == delegate) {
      if (use_animation) {
        // The View will be removed once the Close animation completes.
        infobar->AnimateClose();
      } else {
        infobar->Close();
      }
      break;
    }
  }
}

void InfoBarContainer::ReplaceInfoBar(InfoBarDelegate* old_delegate,
                                      InfoBarDelegate* new_delegate) {
  RemoveInfoBar(old_delegate, false);  // no animation
  AddInfoBar(new_delegate, false);  // no animation
}
