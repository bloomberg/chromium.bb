// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_SHARED_MEMORY_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_SHARED_MEMORY_H_

#include "content/browser/device_orientation/data_fetcher_shared_memory_base.h"

namespace content {

class CONTENT_EXPORT DataFetcherSharedMemory
    : public DataFetcherSharedMemoryBase {

 public:
  DataFetcherSharedMemory();
  virtual ~DataFetcherSharedMemory();

 private:

  virtual bool Start(ConsumerType consumer_type) OVERRIDE;
  virtual bool Stop(ConsumerType consumer_type) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(DataFetcherSharedMemory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_DATA_FETCHER_SHARED_MEMORY_H_
