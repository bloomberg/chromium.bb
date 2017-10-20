// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/adaptor/browser_commands_adaptor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BrowserCommandsAdaptor

@synthesize viewControllerForAlert = _viewControllerForAlert;

- (void)closeCurrentTab {
  [self showAlert:@"closeCurrentTab"];
}

- (void)goBack {
  [self showAlert:@"goBack"];
}

- (void)goForward {
  [self showAlert:@"goForward"];
}

- (void)stopLoading {
  [self showAlert:@"stopLoading"];
}

- (void)reload {
  [self showAlert:@"reload"];
}

- (void)bookmarkPage {
  [self showAlert:@"bookmarkPage"];
}

- (void)showToolsMenu {
  [self showAlert:@"showToolsMenu"];
}

- (void)openNewTab:(OpenNewTabCommand*)newTabCommand {
  [self showAlert:@"openNewTab:"];
}

- (void)printTab {
  [self showAlert:@"printTab"];
}

- (void)addToReadingList:(ReadingListAddCommand*)command {
  [self showAlert:@"addToReadingList:"];
}

- (void)showReadingList {
  [self showAlert:@"showReadingList"];
}

- (void)preloadVoiceSearch {
  [self showAlert:@"preloadVoiceSearch"];
}

- (void)closeAllTabs {
  [self showAlert:@"closeAllTabs"];
}

- (void)closeAllIncognitoTabs {
  [self showAlert:@"closeAllIncognitoTabs"];
}

#if !defined(NDEBUG)
- (void)viewSource {
  [self showAlert:@"viewSource"];
}
#endif

- (void)showRateThisAppDialog {
  [self showAlert:@"showRateThisAppDialog"];
}

- (void)showFindInPage {
  [self showAlert:@"showFindInPage"];
}

- (void)closeFindInPage {
  [self showAlert:@"closeFindInPage"];
}

- (void)searchFindInPage {
  [self showAlert:@"searchFindInPage"];
}

- (void)findNextStringInPage {
  [self showAlert:@"findNextStringInPage"];
}

- (void)findPreviousStringInPage {
  [self showAlert:@"findPreviousStringInPage"];
}

- (void)showPageInfoForOriginPoint:(CGPoint)originPoint {
  [self showAlert:@"showPageInfoForOriginPoint:"];
}

- (void)hidePageInfo {
  [self showAlert:@"hidePageInfo"];
}

- (void)showSecurityHelpPage {
  [self showAlert:@"showSecurityHelpPage"];
}

- (void)showHelpPage {
  [self showAlert:@"showHelpPage"];
}

- (void)showBookmarksManager {
  [self showAlert:@"showBookmarksManager"];
}

- (void)showRecentTabs {
  [self showAlert:@"showRecentTabs"];
}

- (void)sharePage {
  [self showAlert:@"sharePage"];
}

- (void)showQRScanner {
  [self showAlert:@"showQRScanner"];
}

- (void)launchExternalSearch {
  [self showAlert:@"launchExternalSearch"];
}

- (void)showTabHistoryPopupForBackwardHistory {
  [self showAlert:@"showTabHistoryPopupForBackwardHistory"];
}

- (void)showTabHistoryPopupForForwardHistory {
  [self showAlert:@"showTabHistoryPopupForForwardHistory"];
}

- (void)navigateToHistoryItem:(const web::NavigationItem*)item {
  [self showAlert:@"navigateToHistoryItem:"];
}

- (void)requestDesktopSite {
  [self showAlert:@"requestDesktopSite"];
}

- (void)requestMobileSite {
  [self showAlert:@"requestMobileSite"];
}

- (void)showSnackbarMessage:(MDCSnackbarMessage*)message {
  [self showAlert:@"showSnackbarMessage:"];
}

#pragma mark - Private

// TODO(crbug.com/740793): Remove this method once no method is using it.
- (void)showAlert:(NSString*)message {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:message
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* action =
      [UIAlertAction actionWithTitle:@"Done"
                               style:UIAlertActionStyleCancel
                             handler:nil];
  [alertController addAction:action];
  [self.viewControllerForAlert presentViewController:alertController
                                            animated:YES
                                          completion:nil];
}

@end
