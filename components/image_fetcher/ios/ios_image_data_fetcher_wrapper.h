// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_IOS_IOS_IMAGE_DATA_FETCHER_WRAPPER_H_
#define COMPONENTS_IMAGE_FETCHER_IOS_IOS_IMAGE_DATA_FETCHER_WRAPPER_H_

#import <Foundation/Foundation.h>

#include "base/memory/ref_counted.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/image_fetcher/image_data_fetcher.h"

namespace base {
class TaskRunner;
}

namespace net {
class URLRequestContextGetter;
}

class GURL;

namespace image_fetcher {

// Callback that informs of the download of an image encoded in |data|.
using IOSImageDataFetcherCallback = void (^)(NSData* data);

class IOSImageDataFetcherWrapper {
 public:
  using DataUseServiceName = data_use_measurement::DataUseUserData::ServiceName;

  // The TaskRunner is used to decode the image if it is WebP-encoded.
  IOSImageDataFetcherWrapper(
      net::URLRequestContextGetter* url_request_context_getter,
      const scoped_refptr<base::TaskRunner>& task_runner);
  virtual ~IOSImageDataFetcherWrapper();

  // Start downloading the image at the given |image_url|. The |callback| will
  // be called with the downloaded image, or nil if any error happened or the
  // http response header is not HTTP_OK (200). If the url is a data URL, the
  // http response header is considered to be HTTP_OK.
  // |callback| cannot be nil.
  virtual void FetchImageDataWebpDecoded(const GURL& image_url,
                                         IOSImageDataFetcherCallback callback);

  // Sets a service name against which to track data usage.
  void SetDataUseServiceName(DataUseServiceName data_use_service_name);

 private:
  // The task runner used to decode images if necessary.
  const scoped_refptr<base::TaskRunner> task_runner_;
  ImageDataFetcher image_data_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(IOSImageDataFetcherWrapper);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_IOS_IOS_IMAGE_DATA_FETCHER_WRAPPER_H_
