// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBBLOBREGISTRY_IMPL_H_
#define CONTENT_CHILD_WEBBLOBREGISTRY_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/platform/WebBlobRegistry.h"

namespace blink {
class WebThreadSafeData;
}  // namespace blink

namespace storage {
class DataElement;
}

namespace content {
class ThreadSafeSender;

class WebBlobRegistryImpl : public blink::WebBlobRegistry {
 public:
  explicit WebBlobRegistryImpl(ThreadSafeSender* sender);
  virtual ~WebBlobRegistryImpl();

  virtual void registerBlobData(const blink::WebString& uuid,
                                const blink::WebBlobData& data);
  virtual void addBlobDataRef(const blink::WebString& uuid);
  virtual void removeBlobDataRef(const blink::WebString& uuid);
  virtual void registerPublicBlobURL(const blink::WebURL&,
                                     const blink::WebString& uuid);
  virtual void revokePublicBlobURL(const blink::WebURL&);

  // Additional support for Streams.
  virtual void registerStreamURL(const blink::WebURL& url,
                                 const blink::WebString& content_type);
  virtual void registerStreamURL(const blink::WebURL& url,
                                 const blink::WebURL& src_url);
  virtual void addDataToStream(const blink::WebURL& url,
                               const char* data, size_t length);
  virtual void flushStream(const blink::WebURL& url);
  virtual void finalizeStream(const blink::WebURL& url);
  virtual void abortStream(const blink::WebURL& url);
  virtual void unregisterStreamURL(const blink::WebURL& url);

 private:
  // Sends the data in the buffer as a blob item, then resets the buffer size.
  void FlushBlobItemBuffer(const std::string& uuid_str,
                           storage::DataElement* data_buffer) const;

  // Adds the item to the consolidating buffer, flushing the buffer if needed.
  // If the item is too big for the buffer, it is sent as Sync messages in
  // shared memory instead.
  void BufferBlobData(const std::string& uuid_str,
                      const blink::WebThreadSafeData& data,
                      storage::DataElement* data_buffer);
  // Sends data that is larger than the threshold.
  void SendOversizedDataForBlob(const std::string& uuid_str,
                                const blink::WebThreadSafeData& data);

  scoped_refptr<ThreadSafeSender> sender_;
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBBLOBREGISTRY_IMPL_H_
