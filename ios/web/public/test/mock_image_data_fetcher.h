// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_IMAGE_FETCHER_MOCK_IMAGE_DATA_FETCHER_H_
#define IOS_WEB_PUBLIC_IMAGE_FETCHER_MOCK_IMAGE_DATA_FETCHER_H_

#import "ios/web/public/image_fetcher/image_data_fetcher.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace web {

// Mocks the ImageDataFetcher utility class, which can be used to asynchronously
// retrieve an image from an URL.
class MockImageDataFetcher : public ImageDataFetcher {
 public:
  explicit MockImageDataFetcher(
      const scoped_refptr<base::TaskRunner>& task_runner);
  ~MockImageDataFetcher() override;

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

}  // namespace web

#endif  // IOS_WEB_PUBLIC_IMAGE_FETCHER_MOCK_IMAGE_DATA_FETCHER_H_
