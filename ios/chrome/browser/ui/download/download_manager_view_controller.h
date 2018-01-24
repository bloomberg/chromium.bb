// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class DownloadManagerViewController;

typedef NS_ENUM(NSInteger, DownloadManagerState) {
  // Download has not started yet.
  kDownloadManagerStateNotStarted = 0,
  // Download is actively progressing.
  kDownloadManagerStateInProgress,
  // Download is completely finished without errors.
  kDownloadManagerStateSuceeded,
  // Download has failed with an error.
  kDownloadManagerStateFailed,
};

@protocol DownloadManagerViewControllerDelegate<NSObject>
@optional
// Called when close button was tapped. Delegate may dismiss presentation.
- (void)downloadManagerViewControllerDidClose:
    (DownloadManagerViewController*)controller;

// Called when Download or Restart button was tapped. Delegate should start the
// download.
- (void)downloadManagerViewControllerDidStartDownload:
    (DownloadManagerViewController*)controller;

// Called when "Open In.." button was tapped. Delegate should present system's
// OpenIn dialog from |layoutGuide|.
- (void)downloadManagerViewController:(DownloadManagerViewController*)controller
     presentOpenInMenuWithLayoutGuide:(UILayoutGuide*)layoutGuide;

@end

// Presents bottom bar UI for a single download task.
@interface DownloadManagerViewController : UIViewController

@property(nonatomic, weak) id<DownloadManagerViewControllerDelegate> delegate;

// Name of the file being downloaded.
@property(nonatomic, copy) NSString* fileName;

// The received size of the file being downloaded in bytes.
@property(nonatomic) int64_t countOfBytesReceived;

// The expected size of the file being downloaded in bytes.
@property(nonatomic) int64_t countOfBytesExpectedToReceive;

// State of the download task. Default is kDownloadManagerStateNotStarted.
@property(nonatomic) DownloadManagerState state;

@end

// All UI elements presend in view controller's view.
@interface DownloadManagerViewController (UIElements)

// Button to dismiss the download toolbar.
@property(nonatomic, readonly) UIButton* closeButton;

// Label that describes the current download status.
@property(nonatomic, readonly) UILabel* statusLabel;

// Button appropriate for the current download status ("Download", "Open In..",
// "Try Again").
@property(nonatomic, readonly) UIButton* actionButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_MANAGER_VIEW_CONTROLLER_H_
