// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/dom_storage_dispatcher.h"

#include "content/common/dom_storage_messages.h"
#include "content/renderer/dom_storage/webstoragearea_impl.h"
#include "content/renderer/dom_storage/webstoragenamespace_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageEventDispatcher.h"

bool DomStorageDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DomStorageDispatcher, msg)
    IPC_MESSAGE_HANDLER(DOMStorageMsg_Event, OnStorageEvent)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DomStorageDispatcher::OnStorageEvent(
    const DOMStorageMsg_Event_Params& params) {
  RenderThreadImpl::current()->EnsureWebKitInitialized();

  bool originated_in_process = params.connection_id != 0;
  WebStorageAreaImpl* originating_area = NULL;
  if (originated_in_process) {
    originating_area = WebStorageAreaImpl::FromConnectionId(
        params.connection_id);
  }

  if (params.namespace_id == dom_storage::kLocalStorageNamespaceId) {
    WebKit::WebStorageEventDispatcher::dispatchLocalStorageEvent(
        params.key,
        params.old_value,
        params.new_value,
        params.origin,
        params.page_url,
        originating_area,
        originated_in_process);
  } else if (originated_in_process) {
    // TODO(michaeln): For now, we only raise session storage events into the
    // process which caused the event to occur. However there are cases where
    // sessions can span process boundaries, so there are correctness issues.
    WebStorageNamespaceImpl
        session_namespace_for_event_dispatch(params.namespace_id);
    WebKit::WebStorageEventDispatcher::dispatchSessionStorageEvent(
        params.key,
        params.old_value,
        params.new_value,
        params.origin,
        params.page_url,
        session_namespace_for_event_dispatch,
        originating_area,
        originated_in_process);
  }
}
