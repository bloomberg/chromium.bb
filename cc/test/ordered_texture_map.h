// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_ORDERED_TEXTURE_MAP_H_
#define CC_TEST_ORDERED_TEXTURE_MAP_H_

#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"

namespace cc {

struct TestTexture;

class OrderedTextureMap {
 public:
  OrderedTextureMap();
  ~OrderedTextureMap();

  void Append(blink::WebGLId id, scoped_refptr<TestTexture> texture);
  void Replace(blink::WebGLId id, scoped_refptr<TestTexture> texture);
  void Remove(blink::WebGLId id);

  size_t Size();

  bool ContainsId(blink::WebGLId id);

  scoped_refptr<TestTexture> TextureForId(blink::WebGLId id);
  blink::WebGLId IdAt(size_t index);

 private:
  typedef base::hash_map<blink::WebGLId, scoped_refptr<TestTexture> >
      TextureMap;
  typedef std::vector<blink::WebGLId> TextureList;

  TextureMap textures_;
  TextureList ordered_textures_;
};

}  // namespace cc

#endif  // CC_TEST_ORDERED_TEXTURE_MAP_H_
