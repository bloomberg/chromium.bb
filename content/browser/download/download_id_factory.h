// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ID_FACTORY_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ID_FACTORY_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "content/browser/download/download_id.h"
#include "content/public/browser/browser_thread.h"

class CONTENT_EXPORT DownloadIdFactory
    : public base::RefCountedThreadSafe<DownloadIdFactory> {
 public:
  // TODO(benjhayden): Instantiate with an explicit next id counter read from
  // persistent storage.
  explicit DownloadIdFactory(DownloadId::Domain domain);

  DownloadId GetNextId();

 private:
  DownloadId::Domain domain_;
  int next_id_;
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(DownloadIdFactory);
};

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ID_FACTORY_H_
