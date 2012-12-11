// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CC_SOLID_COLOR_LAYER_H_
#define CC_SOLID_COLOR_LAYER_H_

#include "cc/cc_export.h"
#include "cc/layer.h"

namespace cc {

// A Layer that renders a solid color. The color is specified by using
// setBackgroundColor() on the base class.
class CC_EXPORT SolidColorLayer : public Layer {
public:
    static scoped_refptr<SolidColorLayer> create();

    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* treeImpl) OVERRIDE;

    virtual void setBackgroundColor(SkColor) OVERRIDE;

protected:
    SolidColorLayer();

private:
    virtual ~SolidColorLayer();
};

}
#endif  // CC_SOLID_COLOR_LAYER_H_
