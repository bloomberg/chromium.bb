// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTextureUpdateQueue_h
#define CCTextureUpdateQueue_h

#include "base/basictypes.h"
#include "cc/texture_copier.h"
#include "cc/texture_uploader.h"
#include <deque>

namespace cc {

class CCTextureUpdateQueue {
public:
    CCTextureUpdateQueue();
    virtual ~CCTextureUpdateQueue();

    void appendFullUpload(TextureUploader::Parameters);
    void appendPartialUpload(TextureUploader::Parameters);
    void appendCopy(TextureCopier::Parameters);

    void clearUploadsToEvictedResources();

    TextureUploader::Parameters takeFirstFullUpload();
    TextureUploader::Parameters takeFirstPartialUpload();
    TextureCopier::Parameters takeFirstCopy();

    size_t fullUploadSize() const { return m_fullEntries.size(); }
    size_t partialUploadSize() const { return m_partialEntries.size(); }
    size_t copySize() const { return m_copyEntries.size(); }

    bool hasMoreUpdates() const;

private:
    void clearUploadsToEvictedResources(std::deque<TextureUploader::Parameters>& entryQueue);
    std::deque<TextureUploader::Parameters> m_fullEntries;
    std::deque<TextureUploader::Parameters> m_partialEntries;
    std::deque<TextureCopier::Parameters> m_copyEntries;

    DISALLOW_COPY_AND_ASSIGN(CCTextureUpdateQueue);
};

}

#endif // CCTextureUpdateQueue_h
