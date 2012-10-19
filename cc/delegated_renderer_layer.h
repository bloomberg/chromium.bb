// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DelegatedRendererLayerChromium_h
#define DelegatedRendererLayerChromium_h

#include "cc/layer.h"

namespace cc {

class DelegatedRendererLayer : public Layer {
public:
    static scoped_refptr<DelegatedRendererLayer> create();

    virtual scoped_ptr<LayerImpl> createLayerImpl() OVERRIDE;

protected:
    DelegatedRendererLayer();

private:
    virtual ~DelegatedRendererLayer();
};

}
#endif
