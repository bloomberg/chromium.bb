// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PICTURE_LAYER_IMPL_H_
#define CC_PICTURE_LAYER_IMPL_H_

#include "cc/layer_impl.h"
#include "cc/picture_pile.h"

namespace cc {

class CC_EXPORT PictureLayerImpl : public LayerImpl {
public:
  static scoped_ptr<PictureLayerImpl> create(int id)
  {
      return make_scoped_ptr(new PictureLayerImpl(id));
  }
  virtual ~PictureLayerImpl();

protected:
  PictureLayerImpl(int id);

  PicturePile pile_;

  friend class PictureLayer;
  DISALLOW_COPY_AND_ASSIGN(PictureLayerImpl);
};

}

#endif  // CC_PICTURE_LAYER_IMPL_H_
