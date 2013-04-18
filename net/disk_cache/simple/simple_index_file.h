// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_FILE_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_FILE_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/port.h"
#include "net/base/net_export.h"
#include "net/disk_cache/simple/simple_index.h"

namespace disk_cache {

const uint64 kSimpleIndexMagicNumber = GG_UINT64_C(0x656e74657220796f);

// Simple Index File format is a pickle serialized data of IndexMetadata and
// EntryMetadata objects.  The file format is as follows: one instance of
// serialized |IndexMetadata| followed serialized |EntryMetadata| entries
// repeated |number_of_entries| amount of times.  To know more about the format,
// see SimpleIndexFile::Serialize() and SeeSimpleIndexFile::LoadFromDisk()
// methods.
class NET_EXPORT_PRIVATE SimpleIndexFile {
 public:
  class NET_EXPORT_PRIVATE IndexMetadata {
   public:
    IndexMetadata();
    IndexMetadata(uint64 number_of_entries, uint64 cache_size);

    void Serialize(Pickle* pickle) const;
    bool Deserialize(PickleIterator* it);

    bool CheckIndexMetadata();

    uint64 GetNumberOfEntries() { return number_of_entries_; }

   private:
    FRIEND_TEST_ALL_PREFIXES(IndexMetadataTest, Basics);
    FRIEND_TEST_ALL_PREFIXES(IndexMetadataTest, Serialize);

    uint64 magic_number_;
    uint32 version_;
    uint64 number_of_entries_;
    uint64 cache_size_;  // Total cache storage size in bytes.
  };

  // Load the index file from disk, deserializing it and returning the
  // corresponding EntrySet in a scoped_ptr<>, if successful.
  // Uppon failure, the scoped_ptr<> will contain NULL.
  static scoped_ptr<SimpleIndex::EntrySet> LoadFromDisk(
      const base::FilePath& index_filename);

  // Returns a scoped_ptr for a newly allocated Pickle containing the serialized
  // data to be written to a file.
  static scoped_ptr<Pickle> Serialize(
      const SimpleIndexFile::IndexMetadata& index_metadata,
      const SimpleIndex::EntrySet& entries);

  // Write the serialized data from |pickle| into the index file.
  static void WriteToDisk(const base::FilePath& index_filename,
                          const Pickle& pickle);

 private:
  FRIEND_TEST_ALL_PREFIXES(SimpleIndexFileTest, Serialize);

  // Deserialize() is separate from LoadFromDisk() for easier testing.
  static scoped_ptr<SimpleIndex::EntrySet> Deserialize(const char* data,
                                                       int data_len);

  struct PickleHeader : public Pickle::Header {
    uint32 crc;
  };

  DISALLOW_COPY_AND_ASSIGN(SimpleIndexFile);
};


}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_INDEX_FILE_H_
