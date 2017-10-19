// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_BODY_H_
#define CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_BODY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "storage/common/data_element.h"
#include "storage/public/interfaces/size_getter.mojom.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include <jni.h>
#include "base/android/scoped_java_ref.h"
#endif

namespace content {

// ResourceRequestBody represents body (i.e. upload data) of a HTTP request.
class CONTENT_EXPORT ResourceRequestBody
    : public base::RefCountedThreadSafe<ResourceRequestBody> {
 public:
  typedef storage::DataElement Element;

  ResourceRequestBody();

  // Creates ResourceRequestBody that holds a copy of |bytes|.
  static scoped_refptr<ResourceRequestBody> CreateFromBytes(const char* bytes,
                                                            size_t length);

#if defined(OS_ANDROID)
  // Returns a org.chromium.content_public.common.ResourceRequestBody Java
  // object that wraps a ref-counted pointer to the |resource_request_body|.
  base::android::ScopedJavaLocalRef<jobject> ToJavaObject(JNIEnv* env);

  // Extracts and returns a C++ object out of Java's
  // org.chromium.content_public.common.ResourceRequestBody.
  static scoped_refptr<ResourceRequestBody> FromJavaObject(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_object);
#endif

  void AppendBytes(const char* bytes, int bytes_len);
  void AppendFileRange(const base::FilePath& file_path,
                       uint64_t offset,
                       uint64_t length,
                       const base::Time& expected_modification_time);

  void AppendBlob(const std::string& uuid);
  void AppendFileSystemFileRange(const GURL& url,
                                 uint64_t offset,
                                 uint64_t length,
                                 const base::Time& expected_modification_time);
  void AppendDataPipe(mojo::ScopedDataPipeConsumerHandle handle,
                      storage::mojom::SizeGetterPtr size_getter);

  const std::vector<Element>* elements() const { return &elements_; }
  std::vector<Element>* elements_mutable() { return &elements_; }
  void swap_elements(std::vector<Element>* elements) {
    elements_.swap(*elements);
  }

  // Identifies a particular upload instance, which is used by the cache to
  // formulate a cache key.  This value should be unique across browser
  // sessions.  A value of 0 is used to indicate an unspecified identifier.
  void set_identifier(int64_t id) { identifier_ = id; }
  int64_t identifier() const { return identifier_; }

  // Returns paths referred to by |elements| of type Element::TYPE_FILE.
  std::vector<base::FilePath> GetReferencedFiles() const;

  // Sets the flag which indicates whether the post data contains sensitive
  // information like passwords.
  void set_contains_sensitive_info(bool contains_sensitive_info) {
    contains_sensitive_info_ = contains_sensitive_info;
  }
  bool contains_sensitive_info() const { return contains_sensitive_info_; }

 private:
  friend class base::RefCountedThreadSafe<ResourceRequestBody>;

  ~ResourceRequestBody();

  std::vector<Element> elements_;
  int64_t identifier_;

  bool contains_sensitive_info_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestBody);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RESOURCE_REQUEST_BODY_H_
