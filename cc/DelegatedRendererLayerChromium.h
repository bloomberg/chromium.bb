// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DelegatedRendererLayerChromium_h
#define DelegatedRendererLayerChromium_h

#include "LayerChromium.h"

namespace cc {

class DelegatedRendererLayerChromium : public LayerChromium {
public:
    static scoped_refptr<DelegatedRendererLayerChromium> create();

    virtual scoped_ptr<CCLayerImpl> createCCLayerImpl() OVERRIDE;

protected:
    DelegatedRendererLayerChromium();

private:
    virtual ~DelegatedRendererLayerChromium();
};

}
#endif
