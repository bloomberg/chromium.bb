// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBBLOBREGISTRY_IMPL_H_
#define CONTENT_CHILD_WEBBLOBREGISTRY_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "content/child/blob_storage/blob_consolidation.h"
#include "storage/common/data_element.h"
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

  // TODO(dmurph): remove this after moving to createBuilder. crbug.com/504583
  virtual void registerBlobData(const blink::WebString& uuid,
                                const blink::WebBlobData& data);

  virtual blink::WebBlobRegistry::Builder* createBuilder(
      const blink::WebString& uuid,
      const blink::WebString& content_type);

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
  // Handles all of the IPCs sent for building a blob.
  class BuilderImpl : public blink::WebBlobRegistry::Builder {
   public:
    BuilderImpl(const blink::WebString& uuid,
                const blink::WebString& contentType,
                ThreadSafeSender* sender);
    virtual ~BuilderImpl();

    virtual void appendData(const blink::WebThreadSafeData&);
    virtual void appendFile(const blink::WebString& path,
                            uint64_t offset,
                            uint64_t length,
                            double expected_modification_time);
    virtual void appendBlob(const blink::WebString& uuid,
                            uint64_t offset,
                            uint64_t length) override;
    virtual void appendFileSystemURL(const blink::WebURL&,
                                     uint64_t offset,
                                     uint64_t length,
                                     double expected_modification_time);

    void build() override;

   private:
    // Sends data that is larger than the threshold.
    void SendOversizedDataForBlob(size_t consolidated_item_index);

    const std::string uuid_;
    const std::string content_type_;
    BlobConsolidation consolidation_;
    scoped_refptr<ThreadSafeSender> sender_;
  };

  scoped_refptr<ThreadSafeSender> sender_;
};

}  // namespace content

#endif  // CONTENT_CHILD_WEBBLOBREGISTRY_IMPL_H_
