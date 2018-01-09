// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_DOWNLOAD_MANAGER_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_TEST_FAKES_FAKE_DOWNLOAD_MANAGER_TAB_HELPER_DELEGATE_H_

#import "ios/chrome/browser/download/download_manager_tab_helper_delegate.h"
#import "ios/web/public/download/download_task.h"

// DownloadManagerTabHelperDelegate which stores the state of download task.
@interface FakeDownloadManagerTabHelperDelegate
    : NSObject<DownloadManagerTabHelperDelegate>

// The state of current download task. null if there is no current download task
// or when DownloadManager's WebState was hidden.
@property(nonatomic, readonly) web::DownloadTask::State* state;

@end

#endif  // IOS_CHROME_TEST_FAKES_FAKE_DOWNLOAD_MANAGER_TAB_HELPER_DELEGATE_H_
