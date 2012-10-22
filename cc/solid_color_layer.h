// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef SolidColorLayerChromium_h
#define SolidColorLayerChromium_h

#include "cc/layer.h"

namespace cc {

// A Layer that renders a solid color. The color is specified by using
// setBackgroundColor() on the base class.
class SolidColorLayer : public Layer {
public:
    static scoped_refptr<SolidColorLayer> create();

    virtual scoped_ptr<LayerImpl> createLayerImpl() OVERRIDE;

protected:
    SolidColorLayer();

private:
    virtual ~SolidColorLayer();
};

}
#endif
