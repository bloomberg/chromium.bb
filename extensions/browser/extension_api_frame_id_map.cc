// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_api_frame_id_map.h"

#include <tuple>

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"

namespace extensions {

namespace {

// The map is accessed on the IO and UI thread, so construct it once and never
// delete it.
base::LazyInstance<ExtensionApiFrameIdMap>::Leaky g_map_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

const int ExtensionApiFrameIdMap::kInvalidFrameId = -1;
const int ExtensionApiFrameIdMap::kTopFrameId = 0;

ExtensionApiFrameIdMap::CachedFrameIdPair::CachedFrameIdPair()
    : frame_id(kInvalidFrameId), parent_frame_id(kInvalidFrameId) {}

ExtensionApiFrameIdMap::CachedFrameIdPair::CachedFrameIdPair(
    int frame_id,
    int parent_frame_id)
    : frame_id(frame_id), parent_frame_id(parent_frame_id) {}

ExtensionApiFrameIdMap::RenderFrameIdKey::RenderFrameIdKey()
    : render_process_id(content::ChildProcessHost::kInvalidUniqueID),
      frame_routing_id(MSG_ROUTING_NONE) {}

ExtensionApiFrameIdMap::RenderFrameIdKey::RenderFrameIdKey(
    int render_process_id,
    int frame_routing_id)
    : render_process_id(render_process_id),
      frame_routing_id(frame_routing_id) {}

ExtensionApiFrameIdMap::FrameIdCallbacks::FrameIdCallbacks()
    : is_iterating(false) {}

ExtensionApiFrameIdMap::FrameIdCallbacks::~FrameIdCallbacks() {}

bool ExtensionApiFrameIdMap::RenderFrameIdKey::operator<(
    const RenderFrameIdKey& other) const {
  return std::tie(render_process_id, frame_routing_id) <
         std::tie(other.render_process_id, other.frame_routing_id);
}

bool ExtensionApiFrameIdMap::RenderFrameIdKey::operator==(
    const RenderFrameIdKey& other) const {
  return render_process_id == other.render_process_id &&
         frame_routing_id == other.frame_routing_id;
}

ExtensionApiFrameIdMap::ExtensionApiFrameIdMap() {}

ExtensionApiFrameIdMap::~ExtensionApiFrameIdMap() {}

// static
ExtensionApiFrameIdMap* ExtensionApiFrameIdMap::Get() {
  return g_map_instance.Pointer();
}

// static
int ExtensionApiFrameIdMap::GetFrameId(content::RenderFrameHost* rfh) {
  if (!rfh)
    return kInvalidFrameId;
  if (rfh->GetParent())
    return rfh->GetFrameTreeNodeId();
  return kTopFrameId;
}

// static
int ExtensionApiFrameIdMap::GetFrameId(
    content::NavigationHandle* navigation_handle) {
  return navigation_handle->IsInMainFrame()
             ? 0
             : navigation_handle->GetFrameTreeNodeId();
}

// static
int ExtensionApiFrameIdMap::GetParentFrameId(content::RenderFrameHost* rfh) {
  return rfh ? GetFrameId(rfh->GetParent()) : kInvalidFrameId;
}

// static
int ExtensionApiFrameIdMap::GetParentFrameId(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame())
    return -1;

  if (navigation_handle->IsParentMainFrame())
    return 0;

  return navigation_handle->GetParentFrameTreeNodeId();
}

// static
content::RenderFrameHost* ExtensionApiFrameIdMap::GetRenderFrameHostById(
    content::WebContents* web_contents,
    int frame_id) {
  // Although it is technically possible to map |frame_id| to a RenderFrameHost
  // without WebContents, we choose to not do that because in the extension API
  // frameIds are only guaranteed to be meaningful in combination with a tabId.
  if (!web_contents)
    return nullptr;

  if (frame_id == kInvalidFrameId)
    return nullptr;

  if (frame_id == kTopFrameId)
    return web_contents->GetMainFrame();

  DCHECK_GE(frame_id, 1);
  return web_contents->FindFrameByFrameTreeNodeId(frame_id);
}

ExtensionApiFrameIdMap::CachedFrameIdPair ExtensionApiFrameIdMap::KeyToValue(
    const RenderFrameIdKey& key) const {
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      key.render_process_id, key.frame_routing_id);
  return CachedFrameIdPair(GetFrameId(rfh), GetParentFrameId(rfh));
}

ExtensionApiFrameIdMap::CachedFrameIdPair
ExtensionApiFrameIdMap::LookupFrameIdOnUI(const RenderFrameIdKey& key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  FrameIdMap::const_iterator frame_id_iter = frame_id_map_.find(key);
  if (frame_id_iter != frame_id_map_.end())
    return frame_id_iter->second;

  CachedFrameIdPair cached_frame_id_pair = KeyToValue(key);
  // Don't save invalid values in the map.
  if (cached_frame_id_pair.frame_id == kInvalidFrameId)
    return cached_frame_id_pair;

  auto kvpair = FrameIdMap::value_type(key, cached_frame_id_pair);
  base::AutoLock lock(frame_id_map_lock_);
  return frame_id_map_.insert(kvpair).first->second;
}

void ExtensionApiFrameIdMap::ReceivedFrameIdOnIO(
    const RenderFrameIdKey& key,
    const CachedFrameIdPair& cached_frame_id_pair) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  FrameIdCallbacksMap::iterator map_iter = callbacks_map_.find(key);
  if (map_iter == callbacks_map_.end()) {
    // Can happen if ReceivedFrameIdOnIO was called after the frame ID was
    // resolved (e.g. via GetFrameIdOnIO), but before PostTaskAndReply replied.
    return;
  }

  FrameIdCallbacks& callbacks = map_iter->second;

  if (callbacks.is_iterating)
    return;
  callbacks.is_iterating = true;

  // Note: Extra items can be appended to |callbacks| during this loop if a
  // callback calls GetFrameIdOnIO().
  for (std::list<FrameIdCallback>::iterator it = callbacks.callbacks.begin();
       it != callbacks.callbacks.end(); ++it) {
    it->Run(cached_frame_id_pair.frame_id,
            cached_frame_id_pair.parent_frame_id);
  }
  callbacks_map_.erase(key);
}

void ExtensionApiFrameIdMap::GetFrameIdOnIO(int render_process_id,
                                            int frame_routing_id,
                                            const FrameIdCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (frame_routing_id <= -1) {
    // frame_routing_id == -2 = MSG_ROUTING_NONE -> not a RenderFrameHost.
    // frame_routing_id == -1 -> should be MSG_ROUTING_NONE, but there are
    // callers that use "-1" for unknown frames.
    // TODO(robwu): Enable assertion when all callers have been fixed.
    // DCHECK_EQ(MSG_ROUTING_NONE, -1);
    callback.Run(kInvalidFrameId, kInvalidFrameId);
    return;
  }
  // A valid routing ID is only meaningful with a valid process ID.
  DCHECK_GE(render_process_id, 0);

  const RenderFrameIdKey key(render_process_id, frame_routing_id);
  CachedFrameIdPair cached_frame_id_pair;
  bool did_find_cached_frame_id_pair = false;

  {
    base::AutoLock lock(frame_id_map_lock_);
    FrameIdMap::const_iterator frame_id_iter = frame_id_map_.find(key);
    if (frame_id_iter != frame_id_map_.end()) {
      // This is very likely to happen because CacheFrameId() is called as soon
      // as the frame is created.
      cached_frame_id_pair = frame_id_iter->second;
      did_find_cached_frame_id_pair = true;
    }
  }

  FrameIdCallbacksMap::iterator map_iter = callbacks_map_.find(key);

  if (did_find_cached_frame_id_pair) {
    // Value already cached, thread hopping is not needed.
    if (map_iter == callbacks_map_.end()) {
      // If the frame ID was cached, then it is likely that there are no pending
      // callbacks. So do not unnecessarily copy the callback, but run it.
      callback.Run(cached_frame_id_pair.frame_id,
                   cached_frame_id_pair.parent_frame_id);
    } else {
      map_iter->second.callbacks.push_back(callback);
      ReceivedFrameIdOnIO(key, cached_frame_id_pair);
    }
    return;
  }

  // The key was seen for the first time (or the frame has been removed).
  // Hop to the UI thread to look up the extension API frame ID.
  callbacks_map_[key].callbacks.push_back(callback);
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&ExtensionApiFrameIdMap::LookupFrameIdOnUI,
                 base::Unretained(this), key),
      base::Bind(&ExtensionApiFrameIdMap::ReceivedFrameIdOnIO,
                 base::Unretained(this), key));
}

void ExtensionApiFrameIdMap::CacheFrameId(content::RenderFrameHost* rfh) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const RenderFrameIdKey key(rfh->GetProcess()->GetID(), rfh->GetRoutingID());
  CacheFrameId(key);
  DCHECK(frame_id_map_.find(key) != frame_id_map_.end());
}

void ExtensionApiFrameIdMap::CacheFrameId(const RenderFrameIdKey& key) {
  LookupFrameIdOnUI(key);
}

void ExtensionApiFrameIdMap::RemoveFrameId(content::RenderFrameHost* rfh) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(rfh);

  const RenderFrameIdKey key(rfh->GetProcess()->GetID(), rfh->GetRoutingID());
  RemoveFrameId(key);
}

void ExtensionApiFrameIdMap::RemoveFrameId(const RenderFrameIdKey& key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::AutoLock lock(frame_id_map_lock_);
  frame_id_map_.erase(key);
}

}  // namespace extensions
