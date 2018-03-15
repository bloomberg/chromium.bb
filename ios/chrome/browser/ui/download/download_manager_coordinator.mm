// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_coordinator.h"

#include <memory>

#import "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/download/download_manager_metric_names.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/google_drive_app_util.h"
#import "ios/chrome/browser/installation_notifier.h"
#import "ios/chrome/browser/store_kit/store_kit_coordinator.h"
#import "ios/chrome/browser/ui/download/download_manager_mediator.h"
#import "ios/chrome/browser/ui/download/download_manager_view_controller.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/download/download_task.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DownloadManagerCoordinator ()<
    ContainedPresenterDelegate,
    DownloadManagerViewControllerDelegate> {
  // View controller for presenting Download Manager UI.
  DownloadManagerViewController* _viewController;
  // A dialog which requests a confirmation from the user.
  UIAlertController* _confirmationDialog;
  // View controller for presenting "Open In.." dialog.
  UIDocumentInteractionController* _openInController;
  DownloadManagerMediator _mediator;
  StoreKitCoordinator* _storeKitCoordinator;
}
@end

@implementation DownloadManagerCoordinator

@synthesize presenter = _presenter;
@synthesize animatesPresentation = _animatesPresentation;
@synthesize downloadTask = _downloadTask;

- (void)dealloc {
  [[InstallationNotifier sharedInstance] unregisterForNotifications:self];
}

- (void)start {
  DCHECK(self.presenter);

  _viewController = [[DownloadManagerViewController alloc] init];
  _viewController.delegate = self;
  _mediator.SetDownloadTask(_downloadTask);
  _mediator.SetConsumer(_viewController);

  self.presenter.baseViewController = self.baseViewController;
  self.presenter.presentedViewController = _viewController;
  self.presenter.delegate = self;

  [self.presenter prepareForPresentation];

  [self.presenter presentAnimated:self.animatesPresentation];
}

- (void)stop {
  if (_viewController) {
    [self.presenter dismissAnimated:YES];
    _viewController = nil;
  }
  [_confirmationDialog dismissViewControllerAnimated:YES completion:nil];
  _confirmationDialog = nil;
  _downloadTask = nullptr;

  [_storeKitCoordinator stop];
  _storeKitCoordinator = nil;
}

- (UIViewController*)viewController {
  return _viewController;
}

#pragma mark - DownloadManagerTabHelperDelegate

- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
               didCreateDownload:(nonnull web::DownloadTask*)download
               webStateIsVisible:(BOOL)webStateIsVisible {
  if (!webStateIsVisible) {
    // Do nothing if a background Tab requested download UI presentation.
    return;
  }

  BOOL replacingExistingDownload = _downloadTask ? YES : NO;
  _downloadTask = download;

  if (replacingExistingDownload) {
    _mediator.SetDownloadTask(_downloadTask);
  } else {
    self.animatesPresentation = YES;
    [self start];
  }
}

- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
         decidePolicyForDownload:(nonnull web::DownloadTask*)download
               completionHandler:(nonnull void (^)(NewDownloadPolicy))handler {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_REPLACE_CONFIRMATION);
  NSString* message = l10n_util::GetNSString(
      IDS_IOS_DOWNLOAD_MANAGER_REPLACE_CONFIRMATION_MESSAGE);
  [self runConfirmationDialogWithTitle:title
                               message:message
                     completionHandler:^(BOOL confirmed) {
                       handler(confirmed ? kNewDownloadPolicyReplace
                                         : kNewDownloadPolicyDiscard);
                     }];
}

- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
                 didHideDownload:(nonnull web::DownloadTask*)download {
  if (!_downloadTask) {
    // TODO(crbug.com/805653): This callback can be called multiple times.
    return;
  }

  DCHECK_EQ(_downloadTask, download);
  [self stop];
}

- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
                 didShowDownload:(nonnull web::DownloadTask*)download {
  DCHECK_NE(_downloadTask, download);
  _downloadTask = download;
  self.animatesPresentation = NO;
  [self start];
}

#pragma mark - ContainedPresenterDelegate

- (void)containedPresenterDidDismiss:(id<ContainedPresenter>)presenter {
  DCHECK(presenter == self.presenter);
}

#pragma mark - DownloadManagerViewControllerDelegate

- (void)downloadManagerViewControllerDidClose:
    (DownloadManagerViewController*)controller {
  if (_downloadTask->GetState() != web::DownloadTask::State::kInProgress) {
    [self cancelDownload];
    return;
  }

  __weak DownloadManagerCoordinator* weakSelf = self;
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_CANCEL_CONFIRMATION);
  [self runConfirmationDialogWithTitle:title
                               message:nil
                     completionHandler:^(BOOL confirmed) {
                       if (confirmed) {
                         [weakSelf cancelDownload];
                       }
                     }];
}

- (void)installDriveForDownloadManagerViewController:
    (DownloadManagerViewController*)controller {
  if (!_storeKitCoordinator) {
    _storeKitCoordinator = [[StoreKitCoordinator alloc]
        initWithBaseViewController:self.baseViewController];
    _storeKitCoordinator.iTunesItemIdentifier =
        kGoogleDriveITunesItemIdentifier;
  }
  [_storeKitCoordinator start];
  [controller setInstallDriveButtonVisible:NO animated:YES];

  [[InstallationNotifier sharedInstance]
      registerForInstallationNotifications:self
                              withSelector:@selector(didInstallGoogleDriveApp)
                                 forScheme:kGoogleDriveAppURLScheme];
}

- (void)downloadManagerViewControllerDidStartDownload:
    (DownloadManagerViewController*)controller {
  _mediator.StartDowloading();
}

- (void)downloadManagerViewController:(DownloadManagerViewController*)controller
     presentOpenInMenuWithLayoutGuide:(UILayoutGuide*)layoutGuide {
  base::FilePath path =
      _downloadTask->GetResponseWriter()->AsFileWriter()->file_path();
  NSURL* URL = [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];
  _openInController =
      [UIDocumentInteractionController interactionControllerWithURL:URL];

  BOOL menuShown =
      [_openInController presentOpenInMenuFromRect:layoutGuide.layoutFrame
                                            inView:layoutGuide.owningView
                                          animated:YES];
  DCHECK(menuShown);
}

#pragma mark - Private

// Cancels the download task and stops the coordinator.
- (void)cancelDownload {
  // |stop| nulls-our _downloadTask and |Cancel| destroys the task. Call |stop|
  // first to perform all coordinator cleanups, but retain |_downloadTask|
  // pointer to destroy the task.
  web::DownloadTask* downloadTask = _downloadTask;
  [self stop];
  downloadTask->Cancel();
}

// Presents UIAlertController with |title|, |message| and two buttons (OK and
// Cancel). |handler| is called with YES if OK button was tapped and with NO
// if Cancel button was tapped.
- (void)runConfirmationDialogWithTitle:(NSString*)title
                               message:(NSString*)message
                     completionHandler:(void (^)(BOOL confirmed))handler {
  _confirmationDialog =
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* OKAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction*) {
                               handler(YES);
                             }];
  [_confirmationDialog addAction:OKAction];

  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction*) {
                               handler(NO);
                             }];
  [_confirmationDialog addAction:cancelAction];

  [self.baseViewController presentViewController:_confirmationDialog
                                        animated:YES
                                      completion:nil];
}

// Called when Google Drive app is installed after starting StoreKitCoordinator.
- (void)didInstallGoogleDriveApp {
  base::RecordAction(
      base::UserMetricsAction(kDownloadManagerGoogleDriveInstalled));
}

@end
