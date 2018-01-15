// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_home_handset_view_controller.h"

#include <memory>

#include "base/logging.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/strings/grit/components_strings.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_edit_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_editor_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller_protected.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_waiting_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_promo_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_table_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

@implementation BookmarkHomeHandsetViewController

- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(ios::ChromeBrowserState*)browserState
                    dispatcher:(id<ApplicationCommands>)dispatcher {
  self = [super initWithLoader:loader
                  browserState:browserState
                    dispatcher:dispatcher];
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  if (self.bookmarks->loaded())
    [self loadBookmarkViews];
  else
    [self loadWaitingView];
}

- (BOOL)prefersStatusBarHidden {
  return NO;
}

- (NSArray*)keyCommands {
  __weak BookmarkHomeHandsetViewController* weakSelf = self;
  return @[ [UIKeyCommand cr_keyCommandWithInput:UIKeyInputEscape
                                   modifierFlags:Cr_UIKeyModifierNone
                                           title:nil
                                          action:^{
                                            [weakSelf navigationBarCancel:nil];
                                          }] ];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleDefault;
}

#pragma mark - Superclass overrides

- (void)loadBookmarkViews {
  [super loadBookmarkViews];
  DCHECK(self.bookmarks->loaded());
  DCHECK([self isViewLoaded]);
}

@end

