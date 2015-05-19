// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_container_ios.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "ios/chrome/browser/infobars/infobar.h"
#include "ios/chrome/browser/infobars/infobar_container_view.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

InfoBarContainerIOS::InfoBarContainerIOS(
    infobars::InfoBarContainer::Delegate* delegate)
    : InfoBarContainer(delegate), delegate_(delegate) {
  DCHECK(delegate);
  container_view_.reset([[InfoBarContainerView alloc] init]);
  [container_view_ setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                       UIViewAutoresizingFlexibleTopMargin];
}

InfoBarContainerIOS::~InfoBarContainerIOS() {
  delegate_ = nullptr;
  RemoveAllInfoBarsForDestruction();
}

InfoBarContainerView* InfoBarContainerIOS::view() {
  return container_view_;
}

void InfoBarContainerIOS::PlatformSpecificAddInfoBar(infobars::InfoBar* infobar,
                                                     size_t position) {
  InfoBarIOS* infobar_ios = static_cast<InfoBarIOS*>(infobar);
  [container_view_ addInfoBar:infobar_ios position:position];
}

void InfoBarContainerIOS::PlatformSpecificRemoveInfoBar(
    infobars::InfoBar* infobar) {
  InfoBarIOS* infobar_ios = static_cast<InfoBarIOS*>(infobar);
  infobar_ios->RemoveView();
  // If total_height() is 0, then the infobar was removed after an animation. In
  // this case, signal the delegate that the state changed.
  // Otherwise, the infobar is being replaced by another one. Do not call the
  // delegate in this case, as the delegate will be updated when the new infobar
  // is added.
  if (infobar->total_height() == 0 && delegate_)
    delegate_->InfoBarContainerStateChanged(false);

  // TODO(rohitrao, jif): [Merge 239355] Upstream InfoBarContainer deletes the
  // infobar. Avoid deleting it here.
  // crbug.com/327290
  // base::MessageLoop::current()->DeleteSoon(FROM_HERE, infobar_ios);
}

void InfoBarContainerIOS::PlatformSpecificInfoBarStateChanged(
    bool is_animating) {
  [container_view_ setUserInteractionEnabled:!is_animating];
  [container_view_ setNeedsLayout];
}

void InfoBarContainerIOS::SuspendInfobars() {
  ios::GetChromeBrowserProvider()->SetUIViewAlphaWithAnimation(container_view_,
                                                               0);
}

void InfoBarContainerIOS::RestoreInfobars() {
  ios::GetChromeBrowserProvider()->SetUIViewAlphaWithAnimation(container_view_,
                                                               1);
}
