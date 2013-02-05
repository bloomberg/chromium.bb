/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_NACL_IO_INODE_POOL_H_
#define LIBRARIES_NACL_IO_INODE_POOL_H_

#include <stdlib.h>
#include <vector>

#include "nacl_io/osstat.h"
#include "pthread.h"
#include "utils/auto_lock.h"


class INodePool {
 public:
  INodePool()
    : max_nodes_(0),
      num_nodes_(0) {
    pthread_mutex_init(&lock_, NULL);
  }
  ~INodePool() {
    pthread_mutex_destroy(&lock_);
  }

  ino_t Acquire() {
    AutoLock lock(&lock_);
    const int INO_CNT = 8;

    // If we run out of INO numbers, then allocate 8 more
    if (inos_.size() == 0) {
      max_nodes_ += INO_CNT;
      // Add eight more to the stack in reverse order, offset by 1
      // since '0' refers to no INO.
      for (int a = 0; a < INO_CNT; a++) {
        inos_.push_back(max_nodes_ - a);
      }
    }

    // Return the INO at the top of the stack.
    int val = inos_.back();
    inos_.pop_back();
    num_nodes_++;
    return val;
  }

  void Release(ino_t ino) {
    AutoLock lock(&lock_);
    inos_.push_back(ino);
    num_nodes_--;
  }

  size_t size() const { return num_nodes_; }
  size_t capacity() const { return max_nodes_; }

 private:
  size_t num_nodes_;
  size_t max_nodes_;
  std::vector<ino_t> inos_;
  pthread_mutex_t lock_;
};

#endif  // LIBRARIES_NACL_IO_INODE_POOL_H_
