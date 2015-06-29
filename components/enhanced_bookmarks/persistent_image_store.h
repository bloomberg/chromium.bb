// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_PERSISTENT_IMAGE_STORE_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_PERSISTENT_IMAGE_STORE_H_

#include "components/enhanced_bookmarks/image_store.h"

#include "base/files/file_path.h"
#include "sql/connection.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"

// The PersistentImageStore is an implementation of ImageStore that persists its
// data on disk.
class PersistentImageStore : public ImageStore {
 public:
  static const char kBookmarkImageStoreDb[];

  // Creates a PersistentImageStore in the directory at the given path.
  explicit PersistentImageStore(const base::FilePath& path);
  bool HasKey(const GURL& page_url) override;
  void Insert(const GURL& page_url,
              scoped_refptr<enhanced_bookmarks::ImageRecord> image) override;
  void Erase(const GURL& page_url) override;
  scoped_refptr<enhanced_bookmarks::ImageRecord> Get(
      const GURL& page_url) override;
  gfx::Size GetSize(const GURL& page_url) override;
  void GetAllPageUrls(std::set<GURL>* urls) override;
  void ClearAll() override;
  int64 GetStoreSizeInBytes() override;

 protected:
  ~PersistentImageStore() override;

 private:
  sql::InitStatus OpenDatabase();

  const base::FilePath path_;
  sql::Connection db_;
  sql::MetaTable meta_table_;

  DISALLOW_COPY_AND_ASSIGN(PersistentImageStore);
};

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_PERSISTENT_IMAGE_STORE_H_
