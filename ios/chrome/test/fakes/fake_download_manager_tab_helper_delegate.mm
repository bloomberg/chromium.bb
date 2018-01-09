// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_download_manager_tab_helper_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeDownloadManagerTabHelperDelegate {
  std::unique_ptr<web::DownloadTask::State> _state;
}

- (web::DownloadTask::State*)state {
  return _state.get();
}

- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
               didCreateDownload:(nonnull web::DownloadTask*)download {
  _state = std::make_unique<web::DownloadTask::State>(download->GetState());
}

- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
               didUpdateDownload:(nonnull web::DownloadTask*)download {
  _state = std::make_unique<web::DownloadTask::State>(download->GetState());
}

- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
                 didHideDownload:(nonnull web::DownloadTask*)download {
  _state = nullptr;
}

- (void)downloadManagerTabHelper:(nonnull DownloadManagerTabHelper*)tabHelper
                 didShowDownload:(nonnull web::DownloadTask*)download {
  _state = std::make_unique<web::DownloadTask::State>(download->GetState());
}

@end
