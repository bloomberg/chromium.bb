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
#import "chrome/browser/ui/cocoa/passwords/credential_item_button.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"

namespace {
constexpr int kAutoSigninToastTimeoutSeconds = 3;
}  // namespace

@interface AutoSigninViewController () {
  base::scoped_nsobject<CredentialItemButton> credentialView_;
  base::scoped_nsobject<AccountAvatarFetcherManager> avatarManager_;
  std::unique_ptr<base::Timer> timer_;
}
- (instancetype)
initWithAvatarManager:(AccountAvatarFetcherManager*)avatarManager
             delegate:(id<BasePasswordsContentViewDelegate>)delegate;
@end

@implementation AutoSigninViewController

- (instancetype)initWithDelegate:
    (id<BasePasswordsContentViewDelegate>)delegate {
  auto request_context = delegate.model->GetProfile()->GetRequestContext();
  base::scoped_nsobject<AccountAvatarFetcherManager> avatarManager(
      [[AccountAvatarFetcherManager alloc]
          initWithRequestContext:request_context]);
  return [self initWithAvatarManager:avatarManager delegate:delegate];
}

- (instancetype)
initWithAvatarManager:(AccountAvatarFetcherManager*)avatarManager
             delegate:(id<BasePasswordsContentViewDelegate>)delegate {
  if ((self = [super initWithDelegate:delegate])) {
    avatarManager_.reset([avatarManager retain]);
    timer_.reset(new base::Timer(false, false));
    __block AutoSigninViewController* weakSelf = self;
    timer_->Start(FROM_HERE,
                  base::TimeDelta::FromSeconds(kAutoSigninToastTimeoutSeconds),
                  base::BindBlock(^{
                    // |weakSelf| is still valid. Otherwise the timer would have
                    // stopped when it was deleted by [self dealloc].
                    [weakSelf.delegate viewShouldDismiss];
                  }));
  }
  return self;
}

- (void)loadView {
  credentialView_.reset([[CredentialItemButton alloc]
        initWithFrame:NSZeroRect
      backgroundColor:[NSColor textBackgroundColor]
           hoverColor:[NSColor textBackgroundColor]]);
  [credentialView_ setTarget:self];
  [credentialView_ setTrackingEnabled:FALSE];
  base::string16 name = GetCredentialLabelsForAccountChooser(
                            self.delegate.model->pending_password())
                            .first;
  [credentialView_ setTitle:l10n_util::GetNSStringF(
                                IDS_MANAGE_PASSWORDS_AUTO_SIGNIN_TITLE, name)];
  [credentialView_ setImage:[CredentialItemButton defaultAvatar]];
  if (self.delegate.model->pending_password().icon_url.is_valid()) {
    [avatarManager_ fetchAvatar:self.delegate.model->pending_password().icon_url
                        forView:credentialView_];
  }
  [credentialView_ sizeToFit];
  NSRect frame = [credentialView_ frame];
  frame.size.height += 2 * kVerticalAvatarMargin;
  [credentialView_ setFrame:frame];
  [self setView:credentialView_];
}

@end
