// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTextureUpdateQueue_h
#define CCTextureUpdateQueue_h

#include "TextureCopier.h"
#include "TextureUploader.h"
#include <wtf/Deque.h>
#include <wtf/Noncopyable.h>

namespace cc {

class CCTextureUpdateQueue {
    WTF_MAKE_NONCOPYABLE(CCTextureUpdateQueue);
public:
    CCTextureUpdateQueue();
    virtual ~CCTextureUpdateQueue();

    void appendFullUpload(TextureUploader::Parameters);
    void appendPartialUpload(TextureUploader::Parameters);
    void appendCopy(TextureCopier::Parameters);

    void clearUploads();

    TextureUploader::Parameters takeFirstFullUpload();
    TextureUploader::Parameters takeFirstPartialUpload();
    TextureCopier::Parameters takeFirstCopy();

    size_t fullUploadSize() const { return m_fullEntries.size(); }
    size_t partialUploadSize() const { return m_partialEntries.size(); }
    size_t copySize() const { return m_copyEntries.size(); }

    bool hasMoreUpdates() const;

private:
    Deque<TextureUploader::Parameters> m_fullEntries;
    Deque<TextureUploader::Parameters> m_partialEntries;
    Deque<TextureCopier::Parameters> m_copyEntries;
};

}

#endif // CCTextureUpdateQueue_h
