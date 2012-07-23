// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_PROGRAM_CACHE_LRU_HELPER_H_
#define GPU_COMMAND_BUFFER_SERVICE_PROGRAM_CACHE_LRU_HELPER_H_

#include <list>
#include <string>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "gpu/gpu_export.h"

namespace gpu {
namespace gles2 {

// LRU helper for the program cache, operates in O(1) time.
// This class uses a linked list with a hash map.  Both copy their string keys,
// so be mindful that keys you insert will be stored again twice in memory.
class GPU_EXPORT ProgramCacheLruHelper {
 public:
  ProgramCacheLruHelper();
  ~ProgramCacheLruHelper();

  // clears the lru queue
  void Clear();
  // returns true if the lru queue is empty
  bool IsEmpty();
  // inserts or refreshes a key in the queue
  void KeyUsed(const std::string& key);
  // Peeks at the next key.  Use IsEmpty() first (if the queue is empty then
  // null is returned).
  const std::string* PeekKey();
  // evicts the next key from the queue.
  void PopKey();

 private:
  typedef std::list<std::string> StringList;
  typedef base::hash_map<std::string,
                         StringList::iterator> IteratorMap;
  StringList queue;
  IteratorMap location_map;

  DISALLOW_COPY_AND_ASSIGN(ProgramCacheLruHelper);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_PROGRAM_CACHE_LRU_HELPER_H_
