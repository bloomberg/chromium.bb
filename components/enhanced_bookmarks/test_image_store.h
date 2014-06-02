// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_TEST_IMAGE_STORE_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_TEST_IMAGE_STORE_H_

#include "components/enhanced_bookmarks/image_store.h"

// The TestImageStore is an implementation of ImageStore that keeps all its
// data in memory. When deallocated all the associations are lost.
// Used in tests.
class TestImageStore : public ImageStore {
 public:
  TestImageStore();
  virtual bool HasKey(const GURL& page_url) OVERRIDE;
  virtual void Insert(const GURL& page_url,
                      const GURL& image_url,
                      const gfx::Image& image) OVERRIDE;
  virtual void Erase(const GURL& page_url) OVERRIDE;
  virtual std::pair<gfx::Image, GURL> Get(const GURL& page_url) OVERRIDE;
  virtual gfx::Size GetSize(const GURL& page_url) OVERRIDE;
  virtual void GetAllPageUrls(std::set<GURL>* urls) OVERRIDE;
  virtual void ClearAll() OVERRIDE;
  virtual int64 GetStoreSizeInBytes() OVERRIDE;

 protected:
  virtual ~TestImageStore();

 private:
  typedef std::map<const GURL, std::pair<gfx::Image, const GURL> > ImageMap;
  ImageMap store_;

  DISALLOW_COPY_AND_ASSIGN(TestImageStore);
};

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_TEST_IMAGE_STORE_H_
