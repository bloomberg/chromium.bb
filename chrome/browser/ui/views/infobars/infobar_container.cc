// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar_container.h"

#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/infobar.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "ui/base/animation/slide_animation.h"

InfoBarContainer::Delegate::~Delegate() {
}

InfoBarContainer::InfoBarContainer(Delegate* delegate)
    : delegate_(delegate),
      tab_contents_(NULL),
      top_arrow_target_height_(InfoBar::kDefaultArrowTargetHeight) {
}

InfoBarContainer::~InfoBarContainer() {
  // RemoveAllInfoBarsForDestruction() should have already cleared our infobars.
  DCHECK(infobars_.empty());
}

void InfoBarContainer::ChangeTabContents(TabContents* contents) {
  registrar_.RemoveAll();

  while (!infobars_.empty()) {
    InfoBar* infobar = infobars_.front();
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
  OnInfoBarStateChanged(false);
}

int InfoBarContainer::GetVerticalOverlap(int* total_height) {
  // Our |total_height| is the sum of the preferred heights of the InfoBars
  // contained within us plus the |vertical_overlap|.
  int vertical_overlap = 0;
  int next_infobar_y = 0;

  for (InfoBars::iterator i(infobars_.begin()); i != infobars_.end(); ++i) {
    InfoBar* infobar = *i;
    next_infobar_y -= infobar->arrow_height();
    vertical_overlap = std::max(vertical_overlap, -next_infobar_y);
    next_infobar_y += infobar->total_height();
  }

  if (total_height)
    *total_height = next_infobar_y + vertical_overlap;
  return vertical_overlap;
}

void InfoBarContainer::SetMaxTopArrowHeight(int height) {
  // Decrease the height by the arrow stroke thickness, which is the separator
  // line height, because the infobar arrow target heights are without-stroke.
  top_arrow_target_height_ = std::min(
      std::max(height - InfoBar::kSeparatorLineHeight, 0),
      InfoBar::kMaximumArrowTargetHeight);
  UpdateInfoBarArrowTargetHeights();
}

void InfoBarContainer::OnInfoBarStateChanged(bool is_animating) {
  if (delegate_)
    delegate_->InfoBarContainerStateChanged(is_animating);
}

void InfoBarContainer::RemoveDelegate(InfoBarDelegate* delegate) {
  tab_contents_->RemoveInfoBar(delegate);
}

void InfoBarContainer::RemoveInfoBar(InfoBar* infobar) {
  InfoBars::iterator infobar_iterator(std::find(infobars_.begin(),
                                                infobars_.end(), infobar));
  DCHECK(infobar_iterator != infobars_.end());
  PlatformSpecificRemoveInfoBar(infobar);
  infobars_.erase(infobar_iterator);
}

void InfoBarContainer::RemoveAllInfoBarsForDestruction() {
  // Before we remove any children, we reset |delegate_|, so that no removals
  // will result in us trying to call
  // delegate_->InfoBarContainerStateChanged().  This is important because at
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
      UpdateInfoBarArrowTargetHeights();
      break;
    }
  }
}

void InfoBarContainer::AddInfoBar(InfoBar* infobar,
                                  bool animate,
                                  CallbackStatus callback_status) {
  DCHECK(std::find(infobars_.begin(), infobars_.end(), infobar) ==
      infobars_.end());
  infobars_.push_back(infobar);
  UpdateInfoBarArrowTargetHeights();
  PlatformSpecificAddInfoBar(infobar);
  if (callback_status == WANT_CALLBACK)
    infobar->set_container(this);
  infobar->Show(animate);
  if (callback_status == NO_CALLBACK)
    infobar->set_container(this);
}

void InfoBarContainer::UpdateInfoBarArrowTargetHeights() {
  for (size_t i = 0; i < infobars_.size(); ++i)
    infobars_[i]->SetArrowTargetHeight(ArrowTargetHeightForInfoBar(i));
}

int InfoBarContainer::ArrowTargetHeightForInfoBar(size_t infobar_index) const {
  if (!delegate_->DrawInfoBarArrows(NULL))
    return 0;
  if (infobar_index == 0)
    return top_arrow_target_height_;
  const ui::SlideAnimation* first_infobar_animation =
      const_cast<const InfoBar*>(infobars_.front())->animation();
  if ((infobar_index > 1) || first_infobar_animation->IsShowing())
    return InfoBar::kDefaultArrowTargetHeight;
  // When the first infobar is animating closed, we animate the second infobar's
  // arrow target height from the default to the top target height.  Note that
  // the animation values here are going from 1.0 -> 0.0 as the top bar closes.
  return top_arrow_target_height_ + static_cast<int>(
      (InfoBar::kDefaultArrowTargetHeight - top_arrow_target_height_) *
          first_infobar_animation->GetCurrentValue());
}
