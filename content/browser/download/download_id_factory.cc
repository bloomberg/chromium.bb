// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_id_factory.h"

#include "content/browser/download/download_id.h"

DownloadIdFactory::DownloadIdFactory(DownloadId::Domain domain)
  : domain_(domain),
    next_id_(0) {
}

DownloadId DownloadIdFactory::GetNextId() {
  base::AutoLock lock(lock_);
  return DownloadId(domain_, next_id_++);
}
