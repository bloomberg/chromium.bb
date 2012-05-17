// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_BUFFER_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_BUFFER_H_
#pragma once

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"

namespace net {
class IOBuffer;
}

namespace content {

typedef std::pair<scoped_refptr<net::IOBuffer>, size_t> ContentElement;
typedef std::vector<ContentElement> ContentVector;

// Helper function for ContentVector.
// Assembles the data from |contents| and returns it in an |IOBuffer|.
// The number of bytes in the |IOBuffer| is returned in |*num_bytes|
// (if |num_bytes| is not NULL).
// If |contents| is empty, returns NULL as an |IOBuffer| is not allowed
// to have 0 bytes.
CONTENT_EXPORT net::IOBuffer* AssembleData(const ContentVector& contents,
                                           size_t* num_bytes);

// |DownloadBuffer| is a thread-safe wrapper around |ContentVector|.
//
// It is created and populated on the IO thread, and passed to the
// FILE thread for writing. In order to avoid flooding the FILE thread with
// too many small write messages, each write is appended to the
// |DownloadBuffer| while waiting for the task to run on the FILE
// thread. Access to the write buffers is synchronized via the lock.
class CONTENT_EXPORT DownloadBuffer
    : public base::RefCountedThreadSafe<DownloadBuffer> {
 public:
  DownloadBuffer();

  // Adds data to the buffers.
  // Returns the number of |IOBuffer|s in the ContentVector.
  size_t AddData(net::IOBuffer* io_buffer, size_t byte_count);

  // Retrieves the ContentVector of buffers, clearing the contents.
  // The caller takes ownership.
  ContentVector* ReleaseContents();

  // Gets the number of |IOBuffers| we have.
  size_t size() const;

 private:
  friend class base::RefCountedThreadSafe<DownloadBuffer>;

  // Do not allow explicit destruction by anyone else.
  ~DownloadBuffer();

  mutable base::Lock lock_;
  ContentVector contents_;

  DISALLOW_COPY_AND_ASSIGN(DownloadBuffer);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_BUFFER_H_
