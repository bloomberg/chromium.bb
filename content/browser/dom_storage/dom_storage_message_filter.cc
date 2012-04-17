// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_message_filter.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/nullable_string16.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/utf_string_conversions.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/common/dom_storage_messages.h"
#include "googleurl/src/gurl.h"
#include "webkit/dom_storage/dom_storage_area.h"
#include "webkit/dom_storage/dom_storage_host.h"
#include "webkit/dom_storage/dom_storage_task_runner.h"

using content::BrowserThread;
using dom_storage::DomStorageTaskRunner;
using WebKit::WebStorageArea;

DOMStorageMessageFilter::DOMStorageMessageFilter(
    int unused,
    DOMStorageContextImpl* context)
    : context_(context->context()),
      is_dispatching_message_(false) {
}

DOMStorageMessageFilter::~DOMStorageMessageFilter() {
  DCHECK(!host_.get());
}

void DOMStorageMessageFilter::InitializeInSequence() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  host_.reset(new dom_storage::DomStorageHost(context_));
  context_->AddEventObserver(this);
}

void DOMStorageMessageFilter::UninitializeInSequence() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  context_->RemoveEventObserver(this);
  host_.reset();
}

void DOMStorageMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserMessageFilter::OnFilterAdded(channel);
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::PRIMARY_SEQUENCE,
      base::Bind(&DOMStorageMessageFilter::InitializeInSequence, this));
}

void DOMStorageMessageFilter::OnFilterRemoved() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserMessageFilter::OnFilterRemoved();
  context_->task_runner()->PostShutdownBlockingTask(
      FROM_HERE,
      DomStorageTaskRunner::PRIMARY_SEQUENCE,
      base::Bind(&DOMStorageMessageFilter::UninitializeInSequence, this));
}

base::TaskRunner* DOMStorageMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  if (IPC_MESSAGE_CLASS(message) == DOMStorageMsgStart)
    return context_->task_runner();
  return NULL;
}

bool DOMStorageMessageFilter::OnMessageReceived(const IPC::Message& message,
                                                bool* message_was_ok) {
  if (IPC_MESSAGE_CLASS(message) != DOMStorageMsgStart)
    return false;
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(host_.get());
  AutoReset<bool> auto_reset(&is_dispatching_message_, true);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(DOMStorageMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(DOMStorageHostMsg_OpenStorageArea, OnOpenStorageArea)
    IPC_MESSAGE_HANDLER(DOMStorageHostMsg_CloseStorageArea, OnCloseStorageArea)
    IPC_MESSAGE_HANDLER(DOMStorageHostMsg_Length, OnLength)
    IPC_MESSAGE_HANDLER(DOMStorageHostMsg_Key, OnKey)
    IPC_MESSAGE_HANDLER(DOMStorageHostMsg_GetItem, OnGetItem)
    IPC_MESSAGE_HANDLER(DOMStorageHostMsg_SetItem, OnSetItem)
    IPC_MESSAGE_HANDLER(DOMStorageHostMsg_RemoveItem, OnRemoveItem)
    IPC_MESSAGE_HANDLER(DOMStorageHostMsg_Clear, OnClear)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DOMStorageMessageFilter::OnOpenStorageArea(int64 namespace_id,
                                                const string16& origin,
                                                int64* connection_id) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  *connection_id = host_->OpenStorageArea(namespace_id, GURL(origin));
}

void DOMStorageMessageFilter::OnCloseStorageArea(int64 connection_id) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  host_->CloseStorageArea(connection_id);
}

void DOMStorageMessageFilter::OnLength(int64 connection_id,
                                       unsigned* length) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  *length = host_->GetAreaLength(connection_id);
}

void DOMStorageMessageFilter::OnKey(int64 connection_id, unsigned index,
                                    NullableString16* key) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  *key = host_->GetAreaKey(connection_id, index);
}

void DOMStorageMessageFilter::OnGetItem(int64 connection_id,
                                        const string16& key,
                                        NullableString16* value) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  *value = host_->GetAreaItem(connection_id, key);
}

void DOMStorageMessageFilter::OnSetItem(
    int64 connection_id, const string16& key,
    const string16& value, const GURL& page_url,
    WebKit::WebStorageArea::Result* result, NullableString16* old_value) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  *old_value = NullableString16(true);
  if (host_->SetAreaItem(connection_id, key, value, page_url, old_value))
    *result = WebKit::WebStorageArea::ResultOK;
  else
    *result = WebKit::WebStorageArea::ResultBlockedByQuota;
}

void DOMStorageMessageFilter::OnRemoveItem(
    int64 connection_id, const string16& key, const GURL& pageUrl,
    NullableString16* old_value) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  string16 old_string_value;
  if (host_->RemoveAreaItem(connection_id, key, pageUrl, &old_string_value))
    *old_value = NullableString16(old_string_value, false);
  else
    *old_value = NullableString16(true);
}

void DOMStorageMessageFilter::OnClear(int64 connection_id, const GURL& page_url,
                                      bool* something_cleared) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  *something_cleared = host_->ClearArea(connection_id, page_url);
}

void DOMStorageMessageFilter::OnDomStorageItemSet(
    const dom_storage::DomStorageArea* area,
    const string16& key,
    const string16& new_value,
    const NullableString16& old_value,
    const GURL& page_url) {
  SendDomStorageEvent(area, page_url,
                      NullableString16(key, false),
                      NullableString16(new_value, false),
                      old_value);
}

void DOMStorageMessageFilter::OnDomStorageItemRemoved(
    const dom_storage::DomStorageArea* area,
    const string16& key,
    const string16& old_value,
    const GURL& page_url) {
  SendDomStorageEvent(area, page_url,
                      NullableString16(key, false),
                      NullableString16(true),
                      NullableString16(old_value, false));
}

void DOMStorageMessageFilter::OnDomStorageAreaCleared(
    const dom_storage::DomStorageArea* area,
    const GURL& page_url) {
  SendDomStorageEvent(area, page_url,
                      NullableString16(true),
                      NullableString16(true),
                      NullableString16(true));
}

void DOMStorageMessageFilter::SendDomStorageEvent(
    const dom_storage::DomStorageArea* area,
    const GURL& page_url,
    const NullableString16& key,
    const NullableString16& new_value,
    const NullableString16& old_value) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  // Only send if this renderer is not the source of the event.
  if (is_dispatching_message_)
    return;
  DOMStorageMsg_Event_Params params;
  // TODO(michaeln): change the origin type to be GURL in ipc params.
  params.origin = UTF8ToUTF16(area->origin().spec());
  params.page_url = page_url;
  params.namespace_id = dom_storage::kLocalStorageNamespaceId;
  params.key = key;
  params.new_value = new_value;
  params.old_value = old_value;
  Send(new DOMStorageMsg_Event(params));
}
