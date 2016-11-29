// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_OPTIONS_H_
#define COMPONENTS_LEVELDB_PROTO_OPTIONS_H_

#include "base/files/file_path.h"

namespace leveldb_proto {

struct Options {
  Options(const base::FilePath& database_dir)
      : database_dir(database_dir), write_buffer_size(0), read_cache_size(0) {}
  Options(const base::FilePath& database_dir,
          size_t write_buffer_size,
          size_t read_cache_size)
      : database_dir(database_dir),
        write_buffer_size(write_buffer_size),
        read_cache_size(read_cache_size) {}

  base::FilePath database_dir;
  size_t write_buffer_size;
  size_t read_cache_size;
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_OPTIONS_H_
