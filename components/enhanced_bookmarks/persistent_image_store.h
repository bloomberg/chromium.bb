// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_PERSISTENT_IMAGE_STORE_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_PERSISTENT_IMAGE_STORE_H_

#include "components/enhanced_bookmarks/image_store.h"

#include "base/files/file_path.h"
#include "sql/connection.h"
#include "sql/init_status.h"

// The PersistentImageStore is an implementation of ImageStore that persists its
// data on disk.
class PersistentImageStore : public ImageStore {
 public:
  // Creates a PersistentImageStore in the directory at the given path.
  explicit PersistentImageStore(const base::FilePath& path);
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
  virtual ~PersistentImageStore();

 private:
  sql::InitStatus OpenDatabase();

  const base::FilePath path_;
  sql::Connection db_;

  DISALLOW_COPY_AND_ASSIGN(PersistentImageStore);
};

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_PERSISTENT_IMAGE_STORE_H_
