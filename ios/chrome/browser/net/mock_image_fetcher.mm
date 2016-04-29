// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/net/mock_image_fetcher.h"

MockImageFetcher::MockImageFetcher(
    const scoped_refptr<base::TaskRunner>& task_runner)
    : ImageFetcher(task_runner) {
}

MockImageFetcher::~MockImageFetcher() {
}
