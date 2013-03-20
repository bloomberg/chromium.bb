// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_update_queue.h"

#include "cc/resources/prioritized_resource.h"

namespace cc {

ResourceUpdateQueue::ResourceUpdateQueue()
{
}

ResourceUpdateQueue::~ResourceUpdateQueue()
{
}

void ResourceUpdateQueue::appendFullUpload(const ResourceUpdate& upload)
{
    m_fullEntries.push_back(upload);
}

void ResourceUpdateQueue::appendPartialUpload(const ResourceUpdate& upload)
{
    m_partialEntries.push_back(upload);
}

void ResourceUpdateQueue::appendCopy(TextureCopier::Parameters copy)
{
    m_copyEntries.push_back(copy);
}

void ResourceUpdateQueue::clearUploadsToEvictedResources()
{
    clearUploadsToEvictedResources(m_fullEntries);
    clearUploadsToEvictedResources(m_partialEntries);
}

void ResourceUpdateQueue::clearUploadsToEvictedResources(std::deque<ResourceUpdate>& entryQueue)
{
    std::deque<ResourceUpdate> temp;
    entryQueue.swap(temp);
    while (temp.size()) {
        ResourceUpdate upload = temp.front();
        temp.pop_front();
        if (!upload.texture->BackingResourceWasEvicted())
            entryQueue.push_back(upload);
    }
}

ResourceUpdate ResourceUpdateQueue::takeFirstFullUpload()
{
    ResourceUpdate first = m_fullEntries.front();
    m_fullEntries.pop_front();
    return first;
}

ResourceUpdate ResourceUpdateQueue::takeFirstPartialUpload()
{
    ResourceUpdate first = m_partialEntries.front();
    m_partialEntries.pop_front();
    return first;
}

TextureCopier::Parameters ResourceUpdateQueue::takeFirstCopy()
{
    TextureCopier::Parameters first = m_copyEntries.front();
    m_copyEntries.pop_front();
    return first;
}

bool ResourceUpdateQueue::hasMoreUpdates() const
{
    return m_fullEntries.size() || m_partialEntries.size() || m_copyEntries.size();
}

}  // namespace cc
