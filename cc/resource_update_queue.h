// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCResourceUpdateQueue_h
#define CCResourceUpdateQueue_h

#include "base/basictypes.h"
#include "cc/resource_update.h"
#include "cc/texture_copier.h"
#include <deque>

namespace cc {

class ResourceUpdateQueue {
public:
    ResourceUpdateQueue();
    virtual ~ResourceUpdateQueue();

    void appendFullUpload(const ResourceUpdate&);
    void appendPartialUpload(const ResourceUpdate&);
    void appendCopy(TextureCopier::Parameters);

    void clearUploadsToEvictedResources();

    ResourceUpdate takeFirstFullUpload();
    ResourceUpdate takeFirstPartialUpload();
    TextureCopier::Parameters takeFirstCopy();

    size_t fullUploadSize() const { return m_fullEntries.size(); }
    size_t partialUploadSize() const { return m_partialEntries.size(); }
    size_t copySize() const { return m_copyEntries.size(); }

    bool hasMoreUpdates() const;

private:
    void clearUploadsToEvictedResources(std::deque<ResourceUpdate>& entryQueue);
    std::deque<ResourceUpdate> m_fullEntries;
    std::deque<ResourceUpdate> m_partialEntries;
    std::deque<TextureCopier::Parameters> m_copyEntries;

    DISALLOW_COPY_AND_ASSIGN(ResourceUpdateQueue);
};

}

#endif // CCResourceUpdateQueue_h
