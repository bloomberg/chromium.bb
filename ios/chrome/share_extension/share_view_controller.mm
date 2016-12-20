// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <MobileCoreServices/MobileCoreServices.h>

#import "ios/chrome/share_extension/share_view_controller.h"

#import "base/ios/block_types.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/share_extension/share_extension_view.h"
#import "ios/chrome/share_extension/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Type for completion handler to fetch the components of the share items.
// |idResponse| type depends on the element beeing fetched.
using ItemBlock = void (^)(id idResponse, NSError* error);

namespace {

// Minimum size around the widget
const CGFloat kShareExtensionMargin = 15;
const CGFloat kShareExtensionMaxWidth = 390;
// Clip the last separator out of the table view.
const CGFloat kScreenShotWidth = 100;
const CGFloat kScreenShotHeight = 100;
const CGFloat kAnimationDuration = 0.3;
const CGFloat kMediumAlpha = 0.5;

}  // namespace

@interface ShareViewController ()<ShareExtensionViewActionTarget> {
  // This constraint the center of the widget to be vertically in the center
  // of the the screen. It has to be modified for the dismissal animation.
  NSLayoutConstraint* _centerYConstraint;

  NSURL* _shareURL;
  NSString* _shareTitle;
  NSExtensionItem* _shareItem;
}

@property(nonatomic, weak) UIView* maskView;
@property(nonatomic, weak) ShareExtensionView* shareView;
@property(nonatomic, assign) app_group::ShareExtensionItemType itemType;

// Creates a files in |app_group::ShareExtensionItemsFolder()| containing a
// serialized NSDictionary.
// If |cancel| is true, |actionType| is ignored.
- (void)queueActionItemURL:(NSURL*)URL
                     title:(NSString*)title
                    action:(app_group::ShareExtensionItemType)actionType
                    cancel:(BOOL)cancel
                completion:(ProceduralBlock)completion;

// Loads all the shared elements from the extension context and update the UI.
- (void)loadElementsFromContext;

// Sets constaints to the widget so that margin are at least
// kShareExtensionMargin points and widget width is closest up to
// kShareExtensionMaxWidth points.
- (void)constrainWidgetWidth;

@end

@implementation ShareViewController

@synthesize maskView = _maskView;
@synthesize shareView = _shareView;
@synthesize itemType = _itemType;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // This view shadows the screen under the share extension.
  UIView* maskView = [[UIView alloc] initWithFrame:CGRectZero];
  [self setMaskView:maskView];
  [self.maskView
      setBackgroundColor:[UIColor colorWithWhite:0 alpha:kMediumAlpha]];
  [self.view addSubview:self.maskView];
  ui_util::ConstrainAllSidesOfViewToView(self.view, self.maskView);

  // This view is the main view of the share extension.
  ShareExtensionView* shareView =
      [[ShareExtensionView alloc] initWithActionTarget:self];
  [self setShareView:shareView];
  [self.view addSubview:self.shareView];

  [self constrainWidgetWidth];

  // Position the widget below the screen. It will be slided up with an
  // animation.
  _centerYConstraint = [[shareView centerYAnchor]
      constraintEqualToAnchor:[self.view centerYAnchor]];
  [_centerYConstraint setConstant:(self.view.frame.size.height +
                                   self.shareView.frame.size.height) /
                                  2];
  [_centerYConstraint setActive:YES];
  [[[shareView centerXAnchor] constraintEqualToAnchor:[self.view centerXAnchor]]
      setActive:YES];

  [self.maskView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [self.shareView setTranslatesAutoresizingMaskIntoConstraints:NO];

  [self loadElementsFromContext];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Center the widget.
  [_centerYConstraint setConstant:0];
  [self.maskView setAlpha:0];
  [UIView animateWithDuration:kAnimationDuration
                   animations:^{
                     [self.maskView setAlpha:1];
                     [self.view layoutIfNeeded];
                   }];
}

#pragma mark - Private methods

- (void)constrainWidgetWidth {
  // Setting the constraints.
  NSDictionary* views = @{ @"share" : self.shareView };

  NSDictionary* metrics = @{
    @"margin" : @(kShareExtensionMargin),
    @"maxWidth" : @(kShareExtensionMaxWidth),
  };

  NSArray* constraints = @[
    // Sets the margin around the share extension.
    @"H:|-(>=margin)-[share(<=maxWidth)]-(>=margin)-|",
    // If the screen is too small, reduce width of widget.
    @"H:[share(==maxWidth@900)]",
  ];

  for (NSString* constraint : constraints) {
    [NSLayoutConstraint
        activateConstraints:[NSLayoutConstraint
                                constraintsWithVisualFormat:constraint
                                                    options:0
                                                    metrics:metrics
                                                      views:views]];
  }

  // |self.shareView| must be as large as possible and in the center of the
  // screen.
  [self.shareView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisHorizontal];
}

- (void)loadElementsFromContext {
  NSString* typeURL = static_cast<NSString*>(kUTTypeURL);
  for (NSExtensionItem* item in self.extensionContext.inputItems) {
    for (NSItemProvider* itemProvider in item.attachments) {
      if ([itemProvider hasItemConformingToTypeIdentifier:typeURL]) {
        ItemBlock URLCompletion = ^(id idURL, NSError* error) {
          NSURL* URL = static_cast<NSURL*>(idURL);
          dispatch_async(dispatch_get_main_queue(), ^{
            _shareItem = [item copy];
            _shareURL = [URL copy];
            _shareTitle = [[[item attributedContentText] string] copy];
            if ([_shareTitle length] == 0) {
              _shareTitle = [URL host];
            }
            [self.shareView setURL:URL];
            [self.shareView setTitle:_shareTitle];
          });

        };
        [itemProvider loadItemForTypeIdentifier:typeURL
                                        options:nil
                              completionHandler:URLCompletion];
        NSDictionary* imageOptions = @{
          NSItemProviderPreferredImageSizeKey : [NSValue
              valueWithCGSize:CGSizeMake(kScreenShotWidth, kScreenShotHeight)]
        };
        ItemBlock ImageCompletion = ^(id item, NSError* error) {

          UIImage* image = static_cast<UIImage*>(item);
          if (image) {
            dispatch_async(dispatch_get_main_queue(), ^{
              [self.shareView setScreenshot:image];
            });
          }

        };
        [itemProvider loadPreviewImageWithOptions:imageOptions
                                completionHandler:ImageCompletion];
      }
    }
  }
}

- (void)dismissAndReturnItem:(NSExtensionItem*)item {
  // Set the Y center constraints so the whole extension slides out of the
  // screen. Constant is relative to the center of the screen.
  // The direction (up or down) is relative to the output (cancel or submit).
  NSInteger direction = item ? -1 : 1;
  [_centerYConstraint setConstant:direction *
                                  (self.view.frame.size.height +
                                   self.shareView.frame.size.height) /
                                  2];
  [UIView animateWithDuration:kAnimationDuration
      animations:^{
        [self.maskView setAlpha:0];
        [self.view layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        NSArray* returnItem = item ? @[ item ] : @[];
        [self.extensionContext completeRequestReturningItems:returnItem
                                           completionHandler:nil];
      }];
}

- (void)queueActionItemURL:(NSURL*)URL
                     title:(NSString*)title
                    action:(app_group::ShareExtensionItemType)actionType
                    cancel:(BOOL)cancel
                completion:(ProceduralBlock)completion {
  NSURL* readingListURL = app_group::ShareExtensionItemsFolder();
  if (![[NSFileManager defaultManager]
          fileExistsAtPath:[readingListURL path]]) {
    [[NSFileManager defaultManager] createDirectoryAtPath:[readingListURL path]
                              withIntermediateDirectories:YES
                                               attributes:nil
                                                    error:nil];
  }
  NSDate* date = [NSDate date];
  NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
  // This format sorts files by alphabetical order.
  [dateFormatter setDateFormat:@"yyyy-MM-dd-HH-mm-ss.SSSSSS"];
  NSTimeZone* timeZone = [NSTimeZone timeZoneWithName:@"UTC"];
  [dateFormatter setTimeZone:timeZone];
  NSString* dateString = [dateFormatter stringFromDate:date];
  NSURL* fileURL =
      [readingListURL URLByAppendingPathComponent:dateString isDirectory:NO];

  NSMutableDictionary* dict = [[NSMutableDictionary alloc] init];
  if (URL)
    [dict setObject:URL forKey:app_group::kShareItemURL];
  if (title)
    [dict setObject:title forKey:app_group::kShareItemTitle];
  [dict setObject:date forKey:app_group::kShareItemDate];

  if (!cancel) {
    NSNumber* entryType = [NSNumber numberWithInteger:actionType];
    [dict setObject:entryType forKey:app_group::kShareItemType];
  }

  [dict setValue:[NSNumber numberWithBool:cancel]
          forKey:app_group::kShareItemCancel];
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:dict];
  [[NSFileManager defaultManager] createFileAtPath:[fileURL path]
                                          contents:data
                                        attributes:nil];
  if (completion) {
    completion();
  }
}

#pragma mark - ShareExtensionViewActionTarget

- (void)shareExtensionViewDidSelectCancel:(id)sender {
  [self queueActionItemURL:nil
                     title:nil
                    action:app_group::READING_LIST_ITEM  // Ignored
                    cancel:YES
                completion:^{
                  [self dismissAndReturnItem:nil];
                }];
}

- (void)shareExtensionViewDidSelectAddToReadingList:(id)sender {
  [self queueActionItemURL:_shareURL
                     title:_shareTitle
                    action:app_group::READING_LIST_ITEM
                    cancel:NO
                completion:^{
                  [self dismissAndReturnItem:_shareItem];
                }];
}

- (void)shareExtensionViewDidSelectAddToBookmarks:(id)sender {
  [self queueActionItemURL:_shareURL
                     title:_shareTitle
                    action:app_group::BOOKMARK_ITEM
                    cancel:NO
                completion:^{
                  [self dismissAndReturnItem:_shareItem];
                }];
}

- (void)shareExtensionView:(id)sender
               typeChanged:(app_group::ShareExtensionItemType)type {
  [self setItemType:type];
}

@end
