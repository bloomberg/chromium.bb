// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/dom_storage_dispatcher_host.h"

#include "base/nullable_string16.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/in_process_webkit/dom_storage_area.h"
#include "chrome/browser/in_process_webkit/dom_storage_context.h"
#include "chrome/browser/in_process_webkit/dom_storage_namespace.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host_notification_task.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "googleurl/src/gurl.h"

using WebKit::WebStorageArea;

DOMStorageDispatcherHost* DOMStorageDispatcherHost::storage_event_host_ = NULL;
const GURL* DOMStorageDispatcherHost::storage_event_url_ = NULL;

DOMStorageDispatcherHost::
ScopedStorageEventContext::ScopedStorageEventContext(
    DOMStorageDispatcherHost* dispatcher_host, const GURL* url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  DCHECK(!storage_event_host_);
  DCHECK(!storage_event_url_);
  storage_event_host_ = dispatcher_host;
  storage_event_url_ = url;
  DCHECK(storage_event_host_);
  DCHECK(storage_event_url_);
}

DOMStorageDispatcherHost::
ScopedStorageEventContext::~ScopedStorageEventContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  DCHECK(storage_event_host_);
  DCHECK(storage_event_url_);
  storage_event_host_ = NULL;
  storage_event_url_ = NULL;
}

DOMStorageDispatcherHost::DOMStorageDispatcherHost(
    ResourceMessageFilter* resource_message_filter,
    WebKitContext* webkit_context)
    : webkit_context_(webkit_context),
      resource_message_filter_(resource_message_filter),
      process_handle_(0),
      process_id_(0) {
  DCHECK(webkit_context_.get());
  DCHECK(resource_message_filter_);
}

DOMStorageDispatcherHost::~DOMStorageDispatcherHost() {
}

void DOMStorageDispatcherHost::Init(int process_id,
                                    base::ProcessHandle process_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(resource_message_filter_);  // Ensure Shutdown() has not been called.
  DCHECK(!process_handle_);  // Make sure Init() has not yet been called.
  DCHECK(process_handle);
  Context()->RegisterDispatcherHost(this);
  process_id_ = process_id;
  process_handle_ = process_handle;
}

void DOMStorageDispatcherHost::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // This is not always true during testing.
  if (process_handle_)
    Context()->UnregisterDispatcherHost(this);
  resource_message_filter_ = NULL;
}

/* static */
void DOMStorageDispatcherHost::DispatchStorageEvent(const NullableString16& key,
    const NullableString16& old_value, const NullableString16& new_value,
    const string16& origin, const GURL& url, bool is_local_storage) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  DCHECK(is_local_storage);  // Only LocalStorage is implemented right now.
  DCHECK(storage_event_host_);
  ViewMsg_DOMStorageEvent_Params params;
  params.key_ = key;
  params.old_value_ = old_value;
  params.new_value_ = new_value;
  params.origin_ = origin;
  params.url_ = *storage_event_url_;  // The url passed in is junk.
  params.storage_type_ = is_local_storage ? DOM_STORAGE_LOCAL
                                          : DOM_STORAGE_SESSION;
  // The storage_event_host_ is the DOMStorageDispatcherHost that is up in the
  // current call stack since it caused the storage event to fire.
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(storage_event_host_,
          &DOMStorageDispatcherHost::OnStorageEvent, params));
}

bool DOMStorageDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                                 bool* msg_is_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(process_handle_);

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(DOMStorageDispatcherHost, message, *msg_is_ok)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageStorageAreaId,
                                    OnStorageAreaId)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageLength, OnLength)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageKey, OnKey)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageGetItem, OnGetItem)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageSetItem, OnSetItem)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageRemoveItem,
                                    OnRemoveItem)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_DOMStorageClear, OnClear)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

int64 DOMStorageDispatcherHost::CloneSessionStorage(int64 original_id) {
  return Context()->CloneSessionStorage(original_id);
}

void DOMStorageDispatcherHost::Send(IPC::Message* message) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    // TODO(jorlow): Even if we successfully post, I believe it's possible for
    //               the task to never run (if the IO thread is already shutting
    //               down).  We may want to handle this case, though
    //               realistically it probably doesn't matter.
    if (!BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE, NewRunnableMethod(
                this, &DOMStorageDispatcherHost::Send, message))) {
      // The IO thread is dead.
      delete message;
    }
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!resource_message_filter_)
    delete message;
  else
    resource_message_filter_->Send(message);
}

void DOMStorageDispatcherHost::OnStorageAreaId(int64 namespace_id,
                                               const string16& origin,
                                               IPC::Message* reply_msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ChromeURLRequestContext* url_request_context =
      resource_message_filter_->GetRequestContextForURL(GURL(origin));
  BrowserThread::PostTask(BrowserThread::WEBKIT, FROM_HERE, NewRunnableMethod(
      this, &DOMStorageDispatcherHost::OnStorageAreaIdWebKit, namespace_id,
      origin, reply_msg,
      make_scoped_refptr(url_request_context->host_content_settings_map())));
}

void DOMStorageDispatcherHost::OnStorageAreaIdWebKit(
    int64 namespace_id, const string16& origin, IPC::Message* reply_msg,
    HostContentSettingsMap* host_content_settings_map) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  DOMStorageNamespace* storage_namespace =
      Context()->GetStorageNamespace(namespace_id, true);
  if (!storage_namespace) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_DOMStorageStorageAreaId::ID, process_handle_);
    delete reply_msg;
    return;
  }
  DOMStorageArea* storage_area = storage_namespace->GetStorageArea(
      origin, host_content_settings_map);
  ViewHostMsg_DOMStorageStorageAreaId::WriteReplyParams(reply_msg,
                                                        storage_area->id());
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnLength(int64 storage_area_id,
                                        IPC::Message* reply_msg) {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(BrowserThread::WEBKIT, FROM_HERE, NewRunnableMethod(
        this, &DOMStorageDispatcherHost::OnLength, storage_area_id, reply_msg));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  DOMStorageArea* storage_area = Context()->GetStorageArea(storage_area_id);
  if (!storage_area) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_DOMStorageLength::ID, process_handle_);
    delete reply_msg;
    return;
  }
  unsigned length = storage_area->Length();
  ViewHostMsg_DOMStorageLength::WriteReplyParams(reply_msg, length);
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnKey(int64 storage_area_id, unsigned index,
                                     IPC::Message* reply_msg) {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(BrowserThread::WEBKIT, FROM_HERE, NewRunnableMethod(
        this, &DOMStorageDispatcherHost::OnKey, storage_area_id, index,
        reply_msg));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  DOMStorageArea* storage_area = Context()->GetStorageArea(storage_area_id);
  if (!storage_area) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_DOMStorageKey::ID, process_handle_);
    delete reply_msg;
    return;
  }
  const NullableString16& key = storage_area->Key(index);
  ViewHostMsg_DOMStorageKey::WriteReplyParams(reply_msg, key);
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnGetItem(int64 storage_area_id,
                                         const string16& key,
                                         IPC::Message* reply_msg) {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(BrowserThread::WEBKIT, FROM_HERE, NewRunnableMethod(
        this, &DOMStorageDispatcherHost::OnGetItem, storage_area_id, key,
        reply_msg));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  DOMStorageArea* storage_area = Context()->GetStorageArea(storage_area_id);
  if (!storage_area) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_DOMStorageGetItem::ID, process_handle_);
    delete reply_msg;
    return;
  }
  const NullableString16& value = storage_area->GetItem(key);
  ViewHostMsg_DOMStorageGetItem::WriteReplyParams(reply_msg, value);
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnSetItem(
    int64 storage_area_id, const string16& key, const string16& value,
    const GURL& url, IPC::Message* reply_msg) {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(BrowserThread::WEBKIT, FROM_HERE, NewRunnableMethod(
        this, &DOMStorageDispatcherHost::OnSetItem, storage_area_id, key, value,
        url, reply_msg));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  DOMStorageArea* storage_area = Context()->GetStorageArea(storage_area_id);
  if (!storage_area) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_DOMStorageSetItem::ID, process_handle_);
    return;
  }

  ScopedStorageEventContext scope(this, &url);
  WebStorageArea::Result result;
  NullableString16 old_value = storage_area->SetItem(key, value, &result, this);

  // If content was blocked, tell the UI to display the blocked content icon.
  if (reply_msg->routing_id() == MSG_ROUTING_CONTROL) {
    DLOG(WARNING) << "setItem was not given a proper routing id";
  } else {
    CallRenderViewHostContentSettingsDelegate(
        process_id_, reply_msg->routing_id(),
        &RenderViewHostDelegate::ContentSettings::OnLocalStorageAccessed,
        url, storage_area->owner()->dom_storage_type(),
        result == WebStorageArea::ResultBlockedByPolicy);
  }

  ViewHostMsg_DOMStorageSetItem::WriteReplyParams(reply_msg, result, old_value);
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnRemoveItem(
    int64 storage_area_id, const string16& key, const GURL& url,
    IPC::Message* reply_msg) {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(BrowserThread::WEBKIT, FROM_HERE, NewRunnableMethod(
        this, &DOMStorageDispatcherHost::OnRemoveItem, storage_area_id, key,
        url, reply_msg));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  DOMStorageArea* storage_area = Context()->GetStorageArea(storage_area_id);
  if (!storage_area) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_DOMStorageRemoveItem::ID, process_handle_);
    return;
  }

  ScopedStorageEventContext scope(this, &url);
  NullableString16 old_value = storage_area->RemoveItem(key);
  ViewHostMsg_DOMStorageRemoveItem::WriteReplyParams(reply_msg, old_value);
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnClear(int64 storage_area_id, const GURL& url,
                                       IPC::Message* reply_msg) {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(BrowserThread::WEBKIT, FROM_HERE, NewRunnableMethod(
        this, &DOMStorageDispatcherHost::OnClear, storage_area_id, url,
        reply_msg));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  DOMStorageArea* storage_area = Context()->GetStorageArea(storage_area_id);
  if (!storage_area) {
    BrowserRenderProcessHost::BadMessageTerminateProcess(
        ViewHostMsg_DOMStorageClear::ID, process_handle_);
    return;
  }

  ScopedStorageEventContext scope(this, &url);
  bool something_cleared = storage_area->Clear();
  ViewHostMsg_DOMStorageClear::WriteReplyParams(reply_msg, something_cleared);
  Send(reply_msg);
}

void DOMStorageDispatcherHost::OnStorageEvent(
    const ViewMsg_DOMStorageEvent_Params& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  const DOMStorageContext::DispatcherHostSet* set =
      Context()->GetDispatcherHostSet();
  DOMStorageContext::DispatcherHostSet::const_iterator cur = set->begin();
  while (cur != set->end()) {
    // The renderer that generates the event handles it itself.
    if (*cur != this)
      (*cur)->Send(new ViewMsg_DOMStorageEvent(params));
    ++cur;
  }
}
