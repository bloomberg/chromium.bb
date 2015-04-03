// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/image_store.h"

#include "url/gurl.h"

ImageStore::ImageStore() {
}

ImageStore::~ImageStore() {
}

void ImageStore::ChangeImageURL(const GURL& from, const GURL& to) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (!HasKey(from))
    return;

  scoped_refptr<enhanced_bookmarks::ImageRecord> record = Get(from);
  Erase(from);
  Insert(to, record);
}
