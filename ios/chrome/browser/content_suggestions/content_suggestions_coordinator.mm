// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/content_suggestions_coordinator.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/content_suggestions/content_suggestions_mediator.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_article_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/url_loader.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSuggestionsCoordinator ()<ContentSuggestionsCommands> {
  ContentSuggestionsMediator* _contentSuggestionsMediator;
}

@property(nonatomic, strong) AlertCoordinator* alertCoordinator;
@property(nonatomic, strong) UINavigationController* navigationController;
@property(nonatomic, strong)
    ContentSuggestionsViewController* suggestionsViewController;

- (void)openNewTabWithURL:(const GURL&)URL incognito:(BOOL)incognito;

@end

@implementation ContentSuggestionsCoordinator

@synthesize alertCoordinator = _alertCoordinator;
@synthesize browserState = _browserState;
@synthesize navigationController = _navigationController;
@synthesize suggestionsViewController = _suggestionsViewController;
@synthesize URLLoader = _URLLoader;
@synthesize visible = _visible;

- (void)start {
  if (self.visible || !self.browserState) {
    // Prevent this coordinator from being started twice in a row or without a
    // browser state.
    return;
  }

  _visible = YES;

  _contentSuggestionsMediator = [[ContentSuggestionsMediator alloc]
      initWithContentService:IOSChromeContentSuggestionsServiceFactory::
                                 GetForBrowserState(self.browserState)];

  self.suggestionsViewController = [[ContentSuggestionsViewController alloc]
      initWithStyle:CollectionViewControllerStyleDefault
         dataSource:_contentSuggestionsMediator];

  self.suggestionsViewController.suggestionCommandHandler = self;
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.suggestionsViewController];

  self.suggestionsViewController.navigationItem.leftBarButtonItem =
      [[UIBarButtonItem alloc]
          initWithTitle:l10n_util::GetNSString(IDS_IOS_SUGGESTIONS_DONE)
                  style:UIBarButtonItemStylePlain
                 target:self
                 action:@selector(stop)];

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [[self.navigationController presentingViewController]
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.navigationController = nil;
  _visible = NO;
}

#pragma mark - ContentSuggestionsCommands

- (void)openReadingList {
}

- (void)openFirstPageOfReadingList {
}

- (void)openFaviconAtIndex:(NSInteger)index {
}

- (void)openURL:(const GURL&)URL {
  // TODO(crbug.com/691979): Add metrics.

  [self.URLLoader loadURL:URL
                 referrer:web::Referrer()
               transition:ui::PAGE_TRANSITION_AUTO_BOOKMARK
        rendererInitiated:NO];

  [self stop];
}

- (void)displayContextMenuForArticle:(ContentSuggestionsArticleItem*)articleItem
                             atPoint:(CGPoint)touchLocation {
  NSString* urlString = base::SysUTF8ToNSString(articleItem.articleURL.spec());
  self.alertCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.navigationController
                           title:articleItem.title
                         message:urlString
                            rect:CGRectMake(touchLocation.x, touchLocation.y, 0,
                                            0)
                            view:self.suggestionsViewController.collectionView];

  __weak ContentSuggestionsCoordinator* weakSelf = self;
  GURL articleURL = articleItem.articleURL;

  NSString* openInNewTabTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);
  [self.alertCoordinator
      addItemWithTitle:openInNewTabTitle
                action:^{
                  // TODO(crbug.com/691979): Add metrics.
                  [weakSelf openNewTabWithURL:articleURL incognito:NO];
                }
                 style:UIAlertActionStyleDefault];

  NSString* openInNewTabIncognitoTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB);
  [self.alertCoordinator
      addItemWithTitle:openInNewTabIncognitoTitle
                action:^{
                  // TODO(crbug.com/691979): Add metrics.
                  [weakSelf openNewTabWithURL:articleURL incognito:YES];
                }
                 style:UIAlertActionStyleDefault];

  NSString* deleteTitle =
      l10n_util::GetNSString(IDS_IOS_CONTENT_SUGGESTIONS_DELETE);
  [self.alertCoordinator addItemWithTitle:deleteTitle
                                   action:^{
                                     // TODO(crbug.com/691979): Add metrics.
                                     [weakSelf removeEntry];
                                   }
                                    style:UIAlertActionStyleDefault];

  [self.alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                                   action:^{
                                     // TODO(crbug.com/691979): Add metrics.
                                   }
                                    style:UIAlertActionStyleCancel];

  [self.alertCoordinator start];
}

#pragma mark - Private

- (void)openNewTabWithURL:(const GURL&)URL incognito:(BOOL)incognito {
  // TODO(crbug.com/691979): Add metrics.

  [self.URLLoader webPageOrderedOpen:URL
                            referrer:web::Referrer()
                         inIncognito:incognito
                        inBackground:NO
                            appendTo:kLastTab];

  [self stop];
}

- (void)removeEntry {
  // TODO(crbug.com/691979): Add metrics.
}

@end
