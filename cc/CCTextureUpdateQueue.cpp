// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "CCTextureUpdateQueue.h"

namespace cc {

CCTextureUpdateQueue::CCTextureUpdateQueue()
{
}

CCTextureUpdateQueue::~CCTextureUpdateQueue()
{
}

void CCTextureUpdateQueue::appendFullUpload(TextureUploader::Parameters upload)
{
    m_fullEntries.append(upload);
}

void CCTextureUpdateQueue::appendPartialUpload(TextureUploader::Parameters upload)
{
    m_partialEntries.append(upload);
}

void CCTextureUpdateQueue::appendCopy(TextureCopier::Parameters copy)
{
    m_copyEntries.append(copy);
}

void CCTextureUpdateQueue::clearUploads()
{
    m_fullEntries.clear();
    m_partialEntries.clear();
}

TextureUploader::Parameters CCTextureUpdateQueue::takeFirstFullUpload()
{
    return m_fullEntries.takeFirst();
}

TextureUploader::Parameters CCTextureUpdateQueue::takeFirstPartialUpload()
{
    return m_partialEntries.takeFirst();
}

TextureCopier::Parameters CCTextureUpdateQueue::takeFirstCopy()
{
    return m_copyEntries.takeFirst();
}

bool CCTextureUpdateQueue::hasMoreUpdates() const
{
    return m_fullEntries.size() || m_partialEntries.size() || m_copyEntries.size();
}

}

#endif // USE(ACCELERATED_COMPOSITING)
