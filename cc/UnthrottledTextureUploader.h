// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnthrottledTextureUploader_h
#define UnthrottledTextureUploader_h

#include "CCResourceProvider.h"
#include "TextureUploader.h"

namespace cc {

class UnthrottledTextureUploader : public TextureUploader {
    WTF_MAKE_NONCOPYABLE(UnthrottledTextureUploader);
public:
    static PassOwnPtr<UnthrottledTextureUploader> create()
    {
        return adoptPtr(new UnthrottledTextureUploader());
    }
    virtual ~UnthrottledTextureUploader() { }

    virtual bool isBusy() OVERRIDE { return false; }
    virtual void beginUploads() OVERRIDE { }
    virtual void endUploads() OVERRIDE { }
    virtual void uploadTexture(CCResourceProvider* resourceProvider, Parameters upload) OVERRIDE { upload.texture->updateRect(resourceProvider, upload.sourceRect, upload.destOffset); }

protected:
    UnthrottledTextureUploader() { }
};

}

#endif
