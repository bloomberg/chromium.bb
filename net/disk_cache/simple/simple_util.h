// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_UTIL_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "net/base/net_export.h"

namespace disk_cache {

namespace simple_util {

// Given a |key| for a (potential) entry in the simple backend and the |index|
// of a stream on that entry, returns the filename in which that stream would be
// stored.
NET_EXPORT_PRIVATE std::string GetFilenameFromKeyAndIndex(
    const std::string& key,
    int index);

// Given the size of a file holding a stream in the simple backend and the size
// of the key to an entry, returns the number of bytes in the stream.
int32 GetDataSizeFromKeyAndFileSize(const std::string& key,
                                    int64 file_size);

// Given the size of a stream in the simple backend and the size of the key to
// an entry, returns the number of bytes in the file.
int64 GetFileSizeFromKeyAndDataSize(const std::string& key,
                                    int32 data_size);

// Given the size of a key to an entry, and an offset into a stream on that
// entry, returns the file offset corresponding to |data_offset|.
int64 GetFileOffsetFromKeyAndDataOffset(const std::string& key,
                                        int data_offset);

}  // namespace simple_backend

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_UTIL_H_
