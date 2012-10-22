// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContentLayerChromiumClient_h
#define ContentLayerChromiumClient_h

class SkCanvas;

namespace cc {
class FloatRect;
class IntRect;

class ContentLayerClient {
public:
    virtual void paintContents(SkCanvas*, const IntRect& clip, FloatRect& opaque) = 0;

protected:
    virtual ~ContentLayerClient() { }
};

}

#endif // ContentLayerChromiumClient_h
