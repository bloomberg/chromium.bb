// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_coordinator.h"

#include <memory>

#include "base/files/file_util.h"
#import "base/logging.h"
#import "base/mac/bind_objc_block.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "ios/chrome/browser/download/download_directory_util.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#import "ios/chrome/browser/ui/download/download_manager_view_controller.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/web/public/download/download_task.h"
#include "ios/web/public/web_thread.h"
#include "net/url_request/url_fetcher_response_writer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::WebThread;

@interface DownloadManagerCoordinator ()<
    ContainedPresenterDelegate,
    DownloadManagerViewControllerDelegate> {
  // View controller for presenting Download Manager UI.
  DownloadManagerViewController* _viewController;
  // View controller for presenting "Open In.." dialog.
  UIDocumentInteractionController* _openInController;
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
  [self updateViewController];

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
               didUpdateDownload:(nonnull web::DownloadTask*)download {
  if (_downloadTask != download) {
    // Do nothing if download was updated for a background Tab.
    return;
  }

  [self updateViewController];
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
  [self startDownload];
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

// Updates presented view controller with web::DownloadTask data.
- (void)updateViewController {
  _viewController.state = [self downloadManagerState];
  _viewController.countOfBytesReceived = _downloadTask->GetReceivedBytes();
  _viewController.countOfBytesExpectedToReceive =
      _downloadTask->GetTotalBytes();
  _viewController.fileName =
      base::SysUTF16ToNSString(_downloadTask->GetSuggestedFilename());
}

// Returns DownloadManagerState for the current download task.
- (DownloadManagerState)downloadManagerState {
  switch (_downloadTask->GetState()) {
    case web::DownloadTask::State::kNotStarted:
      return kDownloadManagerStateNotStarted;
    case web::DownloadTask::State::kInProgress:
      return kDownloadManagerStateInProgress;
    case web::DownloadTask::State::kComplete:
      return _downloadTask->GetErrorCode() ? kDownloadManagerStateFailed
                                           : kDownloadManagerStateSuceeded;
    case web::DownloadTask::State::kCancelled:
      // Download Manager should dismiss the UI after download cancellation.
      NOTREACHED();
      return kDownloadManagerStateNotStarted;
  }
}

// Asynchronously starts download operation.
- (void)startDownload {
  base::FilePath downloadDir;
  if (!GetDownloadsDirectory(&downloadDir)) {
    [self didFailFileWriterCreation];
    return;
  }

  // Download will start once writer is created by background task, however it
  // OK to change view controller state now to preven further user interactions
  // with "Start Download" button.
  _viewController.state = kDownloadManagerStateInProgress;

  base::string16 suggestedFileName = _downloadTask->GetSuggestedFilename();
  __weak DownloadManagerCoordinator* weakSelf = self;
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindBlockArc(^{
        if (!weakSelf)
          return;

        if (!base::CreateDirectory(downloadDir)) {
          WebThread::PostTask(WebThread::UI, FROM_HERE, base::BindBlockArc(^{
                                [weakSelf didFailFileWriterCreation];
                              }));
          return;
        }

        base::FilePath downloadFilePath =
            downloadDir.Append(base::UTF16ToUTF8(suggestedFileName));
        auto taskRunner = base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::BACKGROUND});
        __block auto writer = std::make_unique<net::URLFetcherFileWriter>(
            taskRunner, downloadFilePath);
        WebThread::PostTask(WebThread::UI, FROM_HERE, base::BindBlockArc(^{
          writer->Initialize(base::BindBlockArc(^(int error) {
            DownloadManagerCoordinator* strongSelf = weakSelf;
            if (!strongSelf)
              return;

            if (!error) {
              strongSelf.downloadTask->Start(std::move(writer));
            } else {
              [strongSelf didFailFileWriterCreation];
            }
          }));
        }));
      }));
}

// Called when coordinator failed to create file writer.
- (void)didFailFileWriterCreation {
  _viewController.state = kDownloadManagerStateFailed;
}

@end
