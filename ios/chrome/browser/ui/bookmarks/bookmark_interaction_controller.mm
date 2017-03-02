// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_interaction_controller.h"

#include <stdint.h>

#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_controller_factory.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_edit_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/browser/ui/url_loader.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ios/web/public/referrer.h"
#include "ui/base/l10n/l10n_util.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {
const int64_t kLastUsedFolderNone = -1;
}  // namespace

@interface BookmarkInteractionController ()<
    BookmarkEditViewControllerDelegate,
    BookmarkHomeViewControllerDelegate> {
  // The browser state of the current user.
  ios::ChromeBrowserState* _currentBrowserState;  // weak

  // The browser state to use, might be different from _currentBrowserState if
  // it is incognito.
  ios::ChromeBrowserState* _browserState;  // weak

  // The designated url loader.
  base::WeakNSProtocol<id<UrlLoader>> _loader;

  // The parent controller on top of which the UI needs to be presented.
  base::WeakNSObject<UIViewController> _parentController;

  base::mac::ObjCPropertyReleaser
      _propertyReleaser_BookmarkInteractionController;
}

// The bookmark model in use.
@property(nonatomic, assign) BookmarkModel* bookmarkModel;

// A reference to the potentially presented bookmark browser.
@property(nonatomic, retain) BookmarkHomeViewController* bookmarkBrowser;

// A reference to the potentially presented single bookmark editor.
@property(nonatomic, retain) BookmarkEditViewController* bookmarkEditor;

// The user wants to bookmark the current tab.
- (void)addBookmarkForTab:(Tab*)tab;

// Builds a controller and brings it on screen.
- (void)presentBookmarkForTab:(Tab*)tab;

// Dismisses the bookmark browser.
- (void)dismissBookmarkBrowserAnimated:(BOOL)animated;

// Dismisses the bookmark editor.
- (void)dismissBookmarkEditorAnimated:(BOOL)animated;

@end

@implementation BookmarkInteractionController

@synthesize bookmarkBrowser = _bookmarkBrowser;
@synthesize bookmarkEditor = _bookmarkEditor;
@synthesize bookmarkModel = _bookmarkModel;

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterInt64Pref(prefs::kIosBookmarkFolderDefault,
                              kLastUsedFolderNone);
}

+ (const BookmarkNode*)folderForNewBookmarksInBrowserState:
    (ios::ChromeBrowserState*)browserState {
  bookmarks::BookmarkModel* bookmarks =
      ios::BookmarkModelFactory::GetForBrowserState(browserState);
  const BookmarkNode* defaultFolder = bookmarks->mobile_node();

  PrefService* prefs = browserState->GetPrefs();
  int64_t node_id = prefs->GetInt64(prefs::kIosBookmarkFolderDefault);
  if (node_id == kLastUsedFolderNone)
    node_id = defaultFolder->id();
  const BookmarkNode* result =
      bookmarks::GetBookmarkNodeByID(bookmarks, node_id);

  if (result)
    return result;

  return defaultFolder;
}

+ (void)setFolderForNewBookmarks:(const BookmarkNode*)folder
                  inBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(folder && folder->is_folder());
  browserState->GetPrefs()->SetInt64(prefs::kIosBookmarkFolderDefault,
                                     folder->id());
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                              loader:(id<UrlLoader>)loader
                    parentController:(UIViewController*)parentController {
  self = [super init];
  if (self) {
    _propertyReleaser_BookmarkInteractionController.Init(
        self, [BookmarkInteractionController class]);
    // Bookmarks are always opened with the main browser state, even in
    // incognito mode.
    _currentBrowserState = browserState;
    _browserState = browserState->GetOriginalChromeBrowserState();
    _loader.reset(loader);
    _parentController.reset(parentController);
    _bookmarkModel =
        ios::BookmarkModelFactory::GetForBrowserState(_browserState);
    DCHECK(_bookmarkModel);
    DCHECK(_parentController);
  }
  return self;
}

- (void)dealloc {
  _bookmarkBrowser.delegate = nil;
  _bookmarkEditor.delegate = nil;
  [super dealloc];
}

- (void)addBookmarkForTab:(Tab*)tab {
  base::RecordAction(base::UserMetricsAction("BookmarkAdded"));
  const BookmarkNode* defaultFolder =
      [[self class] folderForNewBookmarksInBrowserState:_browserState];
  self.bookmarkModel->AddURL(defaultFolder, defaultFolder->child_count(),
                             base::SysNSStringToUTF16(tab.title), tab.url);

  MDCSnackbarMessageAction* action =
      [[[MDCSnackbarMessageAction alloc] init] autorelease];
  base::WeakNSObject<BookmarkInteractionController> weakSelf(self);
  base::WeakNSObject<Tab> weakTab(tab);
  action.handler = ^{
    base::scoped_nsobject<BookmarkInteractionController> strongSelf(
        [weakSelf retain]);
    if (!strongSelf || !weakTab)
      return;
    [strongSelf presentBookmarkForTab:weakTab];
  };
  action.title = l10n_util::GetNSString(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON);
  action.accessibilityIdentifier = @"Edit";

  NSString* folderTitle =
      bookmark_utils_ios::TitleForBookmarkNode(defaultFolder);
  NSString* text =
      _browserState->GetPrefs()->GetInt64(prefs::kIosBookmarkFolderDefault) !=
              kLastUsedFolderNone
          ? l10n_util::GetNSStringF(IDS_IOS_BOOKMARK_PAGE_SAVED_FOLDER,
                                    base::SysNSStringToUTF16(folderTitle))
          : l10n_util::GetNSString(IDS_IOS_BOOKMARK_PAGE_SAVED);
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.action = action;
  message.category = bookmark_utils_ios::kBookmarksSnackbarCategory;
  [MDCSnackbarManager showMessage:message];
}

- (void)presentBookmarkForTab:(Tab*)tab {
  DCHECK(!self.bookmarkBrowser && !self.bookmarkEditor);
  DCHECK(tab);

  const BookmarkNode* bookmark =
      self.bookmarkModel->GetMostRecentlyAddedUserNodeForURL(tab.url);
  if (!bookmark)
    return;

  [self dismissSnackbar];

  base::scoped_nsobject<BookmarkEditViewController> bookmarkEditor(
      [[BookmarkEditViewController alloc] initWithBookmark:bookmark
                                              browserState:_browserState]);
  self.bookmarkEditor = bookmarkEditor;
  self.bookmarkEditor.delegate = self;
  base::scoped_nsobject<UINavigationController> navController(
      [[BookmarkNavigationController alloc]
          initWithRootViewController:self.bookmarkEditor]);
  navController.get().modalPresentationStyle = UIModalPresentationFormSheet;
  [_parentController presentViewController:navController
                                  animated:YES
                                completion:nil];
}

- (void)presentBookmarkForTab:(Tab*)tab
          currentlyBookmarked:(BOOL)bookmarked
                       inView:(UIView*)parentView
                   originRect:(CGRect)origin {
  if (!self.bookmarkModel->loaded())
    return;
  if (!tab)
    return;

  if (bookmarked)
    [self presentBookmarkForTab:tab];
  else
    [self addBookmarkForTab:tab];
}

- (void)presentBookmarks {
  DCHECK(!self.bookmarkBrowser && !self.bookmarkEditor);
  base::scoped_nsobject<BookmarkControllerFactory> bookmarkControllerFactory(
      [[BookmarkControllerFactory alloc] init]);
  self.bookmarkBrowser = [bookmarkControllerFactory
      bookmarkControllerWithBrowserState:_currentBrowserState
                                  loader:_loader];
  self.bookmarkBrowser.delegate = self;
  self.bookmarkBrowser.modalPresentationStyle = UIModalPresentationFormSheet;
  [_parentController presentViewController:self.bookmarkBrowser
                                  animated:YES
                                completion:nil];
}

- (void)dismissBookmarkBrowserAnimated:(BOOL)animated {
  if (!self.bookmarkBrowser)
    return;

  [self.bookmarkBrowser dismissModals:animated];
  [_parentController dismissViewControllerAnimated:animated
                                        completion:^{
                                          self.bookmarkBrowser.delegate = nil;
                                          self.bookmarkBrowser = nil;
                                        }];
}

- (void)dismissBookmarkEditorAnimated:(BOOL)animated {
  if (!self.bookmarkEditor)
    return;

  [_parentController dismissViewControllerAnimated:animated
                                        completion:^{
                                          self.bookmarkEditor.delegate = nil;
                                          self.bookmarkEditor = nil;
                                        }];
}

- (void)dismissBookmarkModalControllerAnimated:(BOOL)animated {
  [self dismissBookmarkBrowserAnimated:animated];
  [self dismissBookmarkEditorAnimated:animated];
}

- (void)dismissSnackbar {
  // Dismiss any bookmark related snackbar this controller could have presented.
  [MDCSnackbarManager dismissAndCallCompletionBlocksWithCategory:
                          bookmark_utils_ios::kBookmarksSnackbarCategory];
}

#pragma mark - BookmarkEditViewControllerDelegate

- (BOOL)bookmarkEditor:(BookmarkEditViewController*)controller
    shoudDeleteAllOccurencesOfBookmark:(const BookmarkNode*)bookmark {
  return YES;
}

- (void)bookmarkEditorWantsDismissal:(BookmarkEditViewController*)controller {
  [self dismissBookmarkEditorAnimated:YES];
}

#pragma mark - BookmarkHomeViewControllerDelegate

- (void)bookmarkHomeViewControllerWantsDismissal:
            (BookmarkHomeViewController*)controller
                                 navigationToUrl:(const GURL&)url {
  [self dismissBookmarkBrowserAnimated:YES];

  if (url != GURL()) {
    new_tab_page_uma::RecordAction(_browserState,
                                   new_tab_page_uma::ACTION_OPENED_BOOKMARK);
    base::RecordAction(
        base::UserMetricsAction("MobileBookmarkManagerEntryOpened"));

    if (url.SchemeIs(url::kJavaScriptScheme)) {  // bookmarklet
      NSString* jsToEval = [base::SysUTF8ToNSString(url.GetContent())
          stringByRemovingPercentEncoding];
      [_loader loadJavaScriptFromLocationBar:jsToEval];
    } else {
      [_loader loadURL:url
                   referrer:web::Referrer()
                 transition:ui::PAGE_TRANSITION_AUTO_BOOKMARK
          rendererInitiated:NO];
    }
  }
}

@end
