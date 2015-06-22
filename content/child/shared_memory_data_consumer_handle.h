// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SHARED_MEMORY_DATA_CONSUMER_HANDLE_H_
#define CONTENT_CHILD_SHARED_MEMORY_DATA_CONSUMER_HANDLE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/child/request_peer.h"
#include "third_party/WebKit/public/platform/WebDataConsumerHandle.h"

namespace content {

// This class is a WebDataConsumerHandle that accepts RequestPeer::ReceivedData.
class CONTENT_EXPORT SharedMemoryDataConsumerHandle final
    : public NON_EXPORTED_BASE(blink::WebDataConsumerHandle) {
 private:
  class Context;

 public:
  enum BackpressureMode {
    kApplyBackpressure,
    kDoNotApplyBackpressure,
  };

  class CONTENT_EXPORT Writer final {
   public:
    Writer(const scoped_refptr<Context>& context, BackpressureMode mode);
    ~Writer();
    // Note: Writer assumes |AddData| is not called in a client's didGetReadable
    // callback. There isn't such assumption for |Close| and |Fail|.
    void AddData(scoped_ptr<RequestPeer::ReceivedData> data);
    void Close();
    // TODO(yhirano): Consider providing error code.
    void Fail();

   private:
    scoped_refptr<Context> context_;
    BackpressureMode mode_;

    DISALLOW_COPY_AND_ASSIGN(Writer);
  };

  class ReaderImpl final : public Reader {
   public:
    ReaderImpl(scoped_refptr<Context> context, Client* client);
    virtual ~ReaderImpl();
    virtual Result read(void* data, size_t size, Flags flags, size_t* readSize);
    virtual Result beginRead(const void** buffer,
                             Flags flags,
                             size_t* available);
    virtual Result endRead(size_t readSize);

   private:
    scoped_refptr<Context> context_;

    DISALLOW_COPY_AND_ASSIGN(ReaderImpl);
 };

  SharedMemoryDataConsumerHandle(BackpressureMode mode,
                                 scoped_ptr<Writer>* writer);
  virtual ~SharedMemoryDataConsumerHandle();

  scoped_ptr<Reader> ObtainReader(Client* client);

 private:
  virtual ReaderImpl* obtainReaderInternal(Client* client);
  const char* debugName() const override;

  scoped_refptr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemoryDataConsumerHandle);
};

}  // namespace content

#endif  // CONTENT_CHILD_SHARED_MEMORY_DATA_CONSUMER_HANDLE_H_
