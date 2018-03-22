// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_coordinator.h"

#include <memory>

#import "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/download/download_manager_metric_names.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/google_drive_app_util.h"
#import "ios/chrome/browser/installation_notifier.h"
#import "ios/chrome/browser/store_kit/store_kit_coordinator.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/download/download_manager_mediator.h"
#import "ios/chrome/browser/ui/download/download_manager_view_controller.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/download/download_task.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Tracks download tasks which were not opened by the user yet. Reports various
// metrics in DownloadTaskObserver callbacks.
class UnopenedDownloadsTracker : public web::DownloadTaskObserver {
 public:
  // Starts tracking this download task.
  void Add(web::DownloadTask* task) { task->AddObserver(this); }
  // Stops tracking this download task.
  void Remove(web::DownloadTask* task) { task->RemoveObserver(this); }
  // DownloadTaskObserver overrides:
  void OnDownloadUpdated(web::DownloadTask* task) override {
    if (task->IsDone()) {
      UMA_HISTOGRAM_ENUMERATION("Download.IOSDownloadFileResult",
                                task->GetErrorCode()
                                    ? DownloadFileResult::Failure
                                    : DownloadFileResult::Completed,
                                DownloadFileResult::Count);
    }
  }
  void OnDownloadDestroyed(web::DownloadTask* task) override {
    // This download task was never open by the user.
    task->RemoveObserver(this);

    if (task->GetState() == web::DownloadTask::State::kInProgress) {
      UMA_HISTOGRAM_ENUMERATION("Download.IOSDownloadFileResult",
                                DownloadFileResult::Other,
                                DownloadFileResult::Count);
    }

    if (task->IsDone() && task->GetErrorCode() == net::OK) {
      UMA_HISTOGRAM_ENUMERATION("Download.IOSDownloadedFileAction",
                                DownloadedFileAction::NoAction,
                                DownloadedFileAction::Count);
    }
  }
};
}  // namespace

@interface DownloadManagerCoordinator ()<
    ContainedPresenterDelegate,
    DownloadManagerViewControllerDelegate,
    UIDocumentInteractionControllerDelegate> {
  // View controller for presenting Download Manager UI.
  DownloadManagerViewController* _viewController;
  // A dialog which requests a confirmation from the user.
  UIAlertController* _confirmationDialog;
  // View controller for presenting "Open In.." dialog.
  UIDocumentInteractionController* _openInController;
  DownloadManagerMediator _mediator;
  StoreKitCoordinator* _storeKitCoordinator;
  // Coordinator for displaying the alert informing the user that no application
  // on the device can open the file. The alert offers the user to install
  // Google Drive app.
  AlertCoordinator* _installDriveAlertCoordinator;
  UnopenedDownloadsTracker _unopenedDownloads;
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
  [_installDriveAlertCoordinator stop];
  _installDriveAlertCoordinator = nil;
}

- (UIViewController*)viewController {
  return _viewController;
}

#pragma mark - DownloadManagerTabHelperDelegate

- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
               didCreateDownload:(nonnull web::DownloadTask*)download
               webStateIsVisible:(BOOL)webStateIsVisible {
  base::RecordAction(base::UserMetricsAction("MobileDownloadFileUIShown"));
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

#pragma mark - UIDocumentInteractionControllerDelegate

- (void)documentInteractionController:
            (UIDocumentInteractionController*)controller
        willBeginSendingToApplication:(NSString*)applicationID {
  DownloadedFileAction action = [applicationID isEqual:kGoogleDriveAppBundleID]
                                    ? DownloadedFileAction::OpenedInDrive
                                    : DownloadedFileAction::OpenedInOtherApp;
  UMA_HISTOGRAM_ENUMERATION("Download.IOSDownloadedFileAction", action,
                            DownloadedFileAction::Count);
  _unopenedDownloads.Remove(_downloadTask);
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
                         UMA_HISTOGRAM_ENUMERATION(
                             "Download.IOSDownloadFileResult",
                             DownloadFileResult::Cancelled,
                             DownloadFileResult::Count);

                         [weakSelf cancelDownload];
                       }
                     }];
}

- (void)installDriveForDownloadManagerViewController:
    (DownloadManagerViewController*)controller {
  [self presentStoreKitForGoogleDriveApp];
}

- (void)downloadManagerViewControllerDidStartDownload:
    (DownloadManagerViewController*)controller {
  if (_downloadTask->GetErrorCode() != net::OK) {
    base::RecordAction(base::UserMetricsAction("MobileDownloadRetryDownload"));
  } else {
    _unopenedDownloads.Add(_downloadTask);
  }
  _mediator.StartDowloading();
}

- (void)downloadManagerViewController:(DownloadManagerViewController*)controller
     presentOpenInMenuWithLayoutGuide:(UILayoutGuide*)layoutGuide {
  base::FilePath path =
      _downloadTask->GetResponseWriter()->AsFileWriter()->file_path();
  NSURL* URL = [NSURL fileURLWithPath:base::SysUTF8ToNSString(path.value())];
  _openInController =
      [UIDocumentInteractionController interactionControllerWithURL:URL];
  _openInController.delegate = self;

  BOOL menuShown =
      [_openInController presentOpenInMenuFromRect:layoutGuide.layoutFrame
                                            inView:layoutGuide.owningView
                                          animated:YES];

  // No application on this device can open the file. Typically happens on
  // iOS 10, where Files app does not exist.
  if (!menuShown) {
    [self didFailOpenInMenuPresentation];
  }
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

// Called when Open In... menu was not presented. This method shows the alert
// which offers the user to install Google Drive app.
- (void)didFailOpenInMenuPresentation {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_UNABLE_TO_OPEN_FILE);
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_NO_APP_MESSAGE);

  _installDriveAlertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                           title:title
                         message:message];

  NSString* googleDriveButtonTitle =
      l10n_util::GetNSString(IDS_IOS_DOWNLOAD_MANAGER_UPLOAD_TO_GOOGLE_DRIVE);
  __weak DownloadManagerCoordinator* weakSelf = self;
  [_installDriveAlertCoordinator
      addItemWithTitle:googleDriveButtonTitle
                action:^{
                  [weakSelf presentStoreKitForGoogleDriveApp];
                }
                 style:UIAlertActionStyleDefault];

  [_installDriveAlertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:nil
                 style:UIAlertActionStyleCancel];

  [_installDriveAlertCoordinator start];
}

// Presents StoreKit dialog for Google Drive application.
- (void)presentStoreKitForGoogleDriveApp {
  if (!_storeKitCoordinator) {
    _storeKitCoordinator = [[StoreKitCoordinator alloc]
        initWithBaseViewController:self.baseViewController];
    _storeKitCoordinator.iTunesItemIdentifier =
        kGoogleDriveITunesItemIdentifier;
  }
  [_storeKitCoordinator start];
  [_viewController setInstallDriveButtonVisible:NO animated:YES];

  [[InstallationNotifier sharedInstance]
      registerForInstallationNotifications:self
                              withSelector:@selector(didInstallGoogleDriveApp)
                                 forScheme:kGoogleDriveAppURLScheme];
}

@end
