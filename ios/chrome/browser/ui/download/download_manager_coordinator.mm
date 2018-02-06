// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_coordinator.h"

#include <memory>

#import "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#import "ios/chrome/browser/ui/download/download_manager_mediator.h"
#import "ios/chrome/browser/ui/download/download_manager_view_controller.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/web/public/download/download_task.h"
#include "net/url_request/url_fetcher_response_writer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DownloadManagerCoordinator ()<
    ContainedPresenterDelegate,
    DownloadManagerViewControllerDelegate> {
  // View controller for presenting Download Manager UI.
  DownloadManagerViewController* _viewController;
  // View controller for presenting "Open In.." dialog.
  UIDocumentInteractionController* _openInController;
  DownloadManagerMediator _mediator;
}
@end

@implementation DownloadManagerCoordinator

@synthesize presenter = _presenter;
@synthesize animatesPresentation = _animatesPresentation;
@synthesize downloadTask = _downloadTask;

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
  _mediator.SetDownloadTask(nullptr);
  _downloadTask = nullptr;
}

#pragma mark - DownloadManagerTabHelperDelegate

- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
               didCreateDownload:(nonnull web::DownloadTask*)download
               webStateIsVisible:(BOOL)vebStateIsVisible {
  if (!vebStateIsVisible) {
    // Do nothing if a background Tab requested download UI presentation.
    return;
  }

  _downloadTask = download;
  self.animatesPresentation = YES;
  [self start];
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
  _downloadTask->Cancel();
  [self stop];
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

@end
