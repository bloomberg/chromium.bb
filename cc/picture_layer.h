// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PICTURE_LAYER_H_
#define CC_PICTURE_LAYER_H_

#include "cc/layer.h"
#include "cc/picture_pile.h"

namespace cc {

class ContentLayerClient;

class CC_EXPORT PictureLayer : public Layer {
public:
  static scoped_refptr<PictureLayer> create(ContentLayerClient*);

  void clearClient() { client_ = 0; }

  // Implement Layer interface
  virtual bool drawsContent() const OVERRIDE;
  virtual scoped_ptr<LayerImpl> createLayerImpl() OVERRIDE;
  virtual void pushPropertiesTo(LayerImpl*) OVERRIDE;

protected:
  explicit PictureLayer(ContentLayerClient*);
  virtual ~PictureLayer();

private:
  ContentLayerClient* client_;
  PicturePile pile_;
};

}  // namespace cc

#endif  // CC_PICTURE_LAYER_H_
