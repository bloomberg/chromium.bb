// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NET_MOCK_IMAGE_FETCHER_H_
#define IOS_CHROME_BROWSER_NET_MOCK_IMAGE_FETCHER_H_

#import "ios/chrome/browser/net/image_fetcher.h"

#include "testing/gmock/include/gmock/gmock.h"

// Mock the ImageFetcher utility class, which can be used to asynchronously
// retrieve an image from an URL.
class MockImageFetcher : public ImageFetcher {
 public:
  explicit MockImageFetcher(const scoped_refptr<base::TaskRunner>& task_runner);
  ~MockImageFetcher() override;

  MOCK_METHOD4(StartDownload,
               void(const GURL& url,
                    ImageFetchedCallback callback,
                    const std::string& referrer,
                    net::URLRequest::ReferrerPolicy referrer_policy));
  MOCK_METHOD2(StartDownload,
               void(const GURL& url, ImageFetchedCallback callback));
  MOCK_METHOD1(SetRequestContextGetter,
               void(const scoped_refptr<net::URLRequestContextGetter>&
                        request_context_getter));
};

#endif  // IOS_CHROME_BROWSER_NET_MOCK_IMAGE_FETCHER_H_
