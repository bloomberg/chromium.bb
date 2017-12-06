// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_IO_CONTEXT_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_IO_CONTEXT_H_

#include <stddef.h>

#include <map>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {
class ChromeBlobStorageContext;
class StoragePartition;

class DevToolsIOContext {
 public:
  class ROStream : public base::RefCountedDeleteOnSequence<ROStream> {
   public:
    enum Status {
      StatusSuccess,
      StatusEOF,
      StatusFailure
    };

    using ReadCallback =
        base::OnceCallback<void(std::unique_ptr<std::string> data,
                                bool base64_encoded,
                                int status)>;

    virtual void Read(off_t position,
                      size_t max_size,
                      ReadCallback callback) = 0;
    virtual void Close(bool invoke_pending_callbacks) = 0;

   protected:
    friend class base::DeleteHelper<content::DevToolsIOContext::ROStream>;
    friend class base::RefCountedDeleteOnSequence<ROStream>;

    explicit ROStream(scoped_refptr<base::SequencedTaskRunner> task_runner);
    virtual ~ROStream() = 0;

    DISALLOW_COPY_AND_ASSIGN(ROStream);
  };

  class RWStream : public ROStream {
   public:
    virtual void Append(std::unique_ptr<std::string> data) = 0;
    virtual const std::string& handle() = 0;

   protected:
    explicit RWStream(scoped_refptr<base::SequencedTaskRunner> task_runner);
    ~RWStream() override = 0;
    DISALLOW_COPY_AND_ASSIGN(RWStream);
  };

  DevToolsIOContext();
  ~DevToolsIOContext();

  scoped_refptr<RWStream> CreateTempFileBackedStream(bool binary);
  scoped_refptr<ROStream> GetByHandle(const std::string& handle);
  scoped_refptr<ROStream> OpenBlob(ChromeBlobStorageContext*,
                                   StoragePartition*,
                                   const std::string& handle,
                                   const std::string& uuid);

  bool Close(const std::string& handle);
  void DiscardAllStreams();
  void OnBlobOpenComplete(const std::string& handle, bool success);

 private:
  using StreamsMap = std::map<std::string, scoped_refptr<ROStream>>;
  StreamsMap streams_;
  base::WeakPtrFactory<DevToolsIOContext> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_IO_CONTEXT_H_
