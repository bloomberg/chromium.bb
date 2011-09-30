// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

// TODO(pkasting): Port Mac to use this.
#if defined(TOOLKIT_VIEWS) || defined(TOOLKIT_GTK)

#include "chrome/browser/infobars/infobar_container.h"

#include <algorithm>

#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_delegate.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "ui/base/animation/slide_animation.h"

InfoBarContainer::Delegate::~Delegate() {
}

InfoBarContainer::InfoBarContainer(Delegate* delegate)
    : delegate_(delegate),
      tab_helper_(NULL),
      top_arrow_target_height_(InfoBar::kDefaultArrowTargetHeight) {
}

InfoBarContainer::~InfoBarContainer() {
  // RemoveAllInfoBarsForDestruction() should have already cleared our infobars.
  DCHECK(infobars_.empty());
}

void InfoBarContainer::ChangeTabContents(InfoBarTabHelper* tab_helper) {
  registrar_.RemoveAll();

  while (!infobars_.empty()) {
    InfoBar* infobar = infobars_.front();
    // Inform the infobar that it's hidden.  If it was already closing, this
    // closes its delegate.
    infobar->Hide(false);
  }

  tab_helper_ = tab_helper;
  if (tab_helper_) {
    Source<InfoBarTabHelper> th_source(tab_helper_);
    registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
                   th_source);
    registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                   th_source);
    registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REPLACED,
                   th_source);

    for (size_t i = 0; i < tab_helper_->infobar_count(); ++i) {
      // As when we removed the infobars above, we prevent callbacks to
      // OnInfoBarAnimated() for each infobar.
      AddInfoBar(
          tab_helper_->GetInfoBarDelegateAt(i)->CreateInfoBar(tab_helper_),
          i, false, NO_CALLBACK);
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
  UpdateInfoBarArrowTargetHeights();
  PlatformSpecificInfoBarStateChanged(is_animating);
}

void InfoBarContainer::RemoveInfoBar(InfoBar* infobar) {
  infobar->set_container(NULL);
  InfoBars::iterator i(std::find(infobars_.begin(), infobars_.end(), infobar));
  DCHECK(i != infobars_.end());
  PlatformSpecificRemoveInfoBar(infobar);
  infobars_.erase(i);
}

void InfoBarContainer::RemoveAllInfoBarsForDestruction() {
  // Before we remove any children, we reset |delegate_|, so that no removals
  // will result in us trying to call
  // delegate_->InfoBarContainerStateChanged().  This is important because at
  // this point |delegate_| may be shutting down, and it's at best unimportant
  // and at worst disastrous to call that.
  delegate_ = NULL;

  // TODO(pkasting): Remove this once TabContentsWrapper calls CloseSoon().
  for (size_t i = infobars_.size(); i > 0; --i)
    infobars_[i - 1]->CloseSoon();

  ChangeTabContents(NULL);
}

void InfoBarContainer::Observe(int type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED:
      AddInfoBar(
          Details<InfoBarAddedDetails>(details)->CreateInfoBar(tab_helper_),
          infobars_.size(), true, WANT_CALLBACK);
      break;

    case chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED: {
      InfoBarRemovedDetails* removed_details =
          Details<InfoBarRemovedDetails>(details).ptr();
      HideInfoBar(removed_details->first, removed_details->second);
      break;
    }

    case chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REPLACED: {
      InfoBarReplacedDetails* replaced_details =
          Details<InfoBarReplacedDetails>(details).ptr();
      AddInfoBar(replaced_details->second->CreateInfoBar(tab_helper_),
          HideInfoBar(replaced_details->first, false), false, WANT_CALLBACK);
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}

size_t InfoBarContainer::HideInfoBar(InfoBarDelegate* delegate,
                                     bool use_animation) {
  // Search for the infobar associated with |delegate|.  We cannot search for
  // |delegate| in |tab_helper_|, because an InfoBar remains alive until its
  // close animation completes, while the delegate is removed from the tab
  // immediately.
  for (InfoBars::iterator i(infobars_.begin()); i != infobars_.end(); ++i) {
    InfoBar* infobar = *i;
    if (infobar->delegate() == delegate) {
      size_t position = i - infobars_.begin();
      // We merely need hide the infobar; it will call back to RemoveInfoBar()
      // itself once it's hidden.
      infobar->Hide(use_animation);
      infobar->CloseSoon();
      UpdateInfoBarArrowTargetHeights();
      return position;
    }
  }
  NOTREACHED();
  return infobars_.size();
}

void InfoBarContainer::AddInfoBar(InfoBar* infobar,
                                  size_t position,
                                  bool animate,
                                  CallbackStatus callback_status) {
  DCHECK(std::find(infobars_.begin(), infobars_.end(), infobar) ==
      infobars_.end());
  DCHECK_LE(position, infobars_.size());
  infobars_.insert(infobars_.begin() + position, infobar);
  UpdateInfoBarArrowTargetHeights();
  PlatformSpecificAddInfoBar(infobar, position);
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
  const ui::SlideAnimation& first_infobar_animation =
      const_cast<const InfoBar*>(infobars_.front())->animation();
  if ((infobar_index > 1) || first_infobar_animation.IsShowing())
    return InfoBar::kDefaultArrowTargetHeight;
  // When the first infobar is animating closed, we animate the second infobar's
  // arrow target height from the default to the top target height.  Note that
  // the animation values here are going from 1.0 -> 0.0 as the top bar closes.
  return top_arrow_target_height_ + static_cast<int>(
      (InfoBar::kDefaultArrowTargetHeight - top_arrow_target_height_) *
          first_infobar_animation.GetCurrentValue());
}

#endif  // TOOLKIT_VIEWS || defined(TOOLKIT_GTK)
