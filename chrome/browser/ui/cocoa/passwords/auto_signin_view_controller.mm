// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#import "chrome/browser/ui/cocoa/passwords/auto_signin_view_controller.h"

#include "base/mac/bind_objc_block.h"
#include "base/strings/sys_string_conversions.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/passwords/account_avatar_fetcher_manager.h"
#import "chrome/browser/ui/cocoa/passwords/credential_item_view.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"

namespace {
const int kAutoSigninToastTimeoutSeconds = 3;
}  // namespace

@interface ManagePasswordsBubbleAutoSigninViewController () <
    CredentialItemDelegate>
- (id)initWithModel:(ManagePasswordsBubbleModel*)model
      avatarManager:(AccountAvatarFetcherManager*)avatarManager
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate;
@end

@implementation ManagePasswordsBubbleAutoSigninViewController

- (id)initWithModel:(ManagePasswordsBubbleModel*)model
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate {
  base::scoped_nsobject<AccountAvatarFetcherManager> avatarManager(
      [[AccountAvatarFetcherManager alloc]
          initWithRequestContext:model->GetProfile()->GetRequestContext()]);
  return
      [self initWithModel:model avatarManager:avatarManager delegate:delegate];
}

- (id)initWithModel:(ManagePasswordsBubbleModel*)model
      avatarManager:(AccountAvatarFetcherManager*)avatarManager
           delegate:(id<ManagePasswordsBubbleContentViewDelegate>)delegate {
  if ((self = [super initWithDelegate:delegate])) {
    model_ = model;
    avatarManager_.reset([avatarManager retain]);
    credentialView_.reset([[CredentialItemView alloc]
        initWithPasswordForm:model->pending_password()
              credentialType:password_manager::CredentialType::
                                 CREDENTIAL_TYPE_PASSWORD
                       style:password_manager_mac::CredentialItemStyle::
                                 AUTO_SIGNIN
                    delegate:self]);
    timer_.reset(new base::Timer(false, false));
    __block ManagePasswordsBubbleAutoSigninViewController* weakSelf = self;
    timer_->Start(FROM_HERE,
                  base::TimeDelta::FromSeconds(kAutoSigninToastTimeoutSeconds),
                  base::BindBlock(^{
                    // |weakSelf| is still valid. Otherwise the timer would have
                    // stopped when it was deleted by [self dealloc].
                    [weakSelf->delegate_ viewShouldDismiss];
                  }));
  }
  return self;
}

- (void)fetchAvatar:(const GURL&)avatarURL forView:(CredentialItemView*)view {
  [avatarManager_ fetchAvatar:avatarURL forView:view];
}

- (void)loadView {
  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);
  const CGFloat kPadding = password_manager::mac::ui::kFramePadding;
  [view setFrameSize:NSMakeSize(
                         2 * kPadding + NSWidth([credentialView_ frame]),
                         2 * kPadding + NSHeight([credentialView_ frame]))];
  [view addSubview:credentialView_];
  [credentialView_ setFrameOrigin:NSMakePoint(kPadding, kPadding)];
  [self setView:view];
}

@end
