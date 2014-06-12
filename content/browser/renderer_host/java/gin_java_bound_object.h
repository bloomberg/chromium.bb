// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_JAVA_GIN_JAVA_BOUND_OBJECT_H_
#define CONTENT_BROWSER_RENDERER_HOST_JAVA_GIN_JAVA_BOUND_OBJECT_H_

#include "base/id_map.h"
#include "base/memory/ref_counted.h"

namespace content {

class GinJavaBoundObject
    : public base::RefCountedThreadSafe<GinJavaBoundObject> {
 public:
  typedef IDMap<scoped_refptr<GinJavaBoundObject>, IDMapOwnPointer> ObjectMap;
  typedef ObjectMap::KeyType ObjectID;

 private:
  friend class base::RefCountedThreadSafe<GinJavaBoundObject>;
  GinJavaBoundObject() {}
  ~GinJavaBoundObject() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_JAVA_GIN_JAVA_BOUND_OBJECT_H_
