// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_BASE_FASTALLOC_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_BASE_FASTALLOC_H_

#include <assert.h>

namespace notifier {

template<class T, size_t Size>
class FastAlloc {
 public:
  FastAlloc() : buffer_(NULL), size_(0) { };
  ~FastAlloc() { freeBuffer(); }
  T* get_buffer(size_t size) {
    if (size_ != 0) {
      // We only allow one call to get_buffer. This makes the logic here
      // simpler, and the user has to worry less about someone else calling
      // get_buffer again on the same FastAlloc object and invalidating the
      // memory they were using.
      assert(false && "get_buffer may one be called once");
      return NULL;
    }

    if (size <= Size) {
      buffer_ = internal_buffer_;
    } else {
      buffer_ = new T[size];
    }

    if (buffer_ != NULL) {
      size_ = size;
    }
    return buffer_;
  }

 private:
  void freeBuffer() {
#if defined(DEBUG)
    memset(buffer_, 0xCC, size_ * sizeof(T));
#endif

    if (buffer_ != NULL && buffer_ != internal_buffer_) {
      delete[] buffer_;
    }
    buffer_ = NULL;
    size_ = 0;
  }

  T* buffer_;
  T internal_buffer_[Size];
  size_t size_;
};

}

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_BASE_FASTALLOC_H_
