// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_io_context.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_request_info.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;

namespace {

// Retrieves the Instant data from the |context|'s named user-data.
InstantIOContext* GetDataForResourceContext(
    content::ResourceContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return base::UserDataAdapter<InstantIOContext>::Get(
      context, InstantIOContext::kInstantIOContextKeyName);
}

InstantIOContext* GetDataForRequest(const net::URLRequest* request) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!info)
    return NULL;

  return GetDataForResourceContext(info->GetContext());
}

}  // namespace

const char InstantIOContext::kInstantIOContextKeyName[] = "instant_io_context";

InstantIOContext::InstantIOContext()
    : most_visited_item_cache_(kMaxInstantMostVisitedItemCacheSize) {
  // The InstantIOContext is created on the UI thread but is accessed
  // on the IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

InstantIOContext::~InstantIOContext() {
}

// static
void InstantIOContext::SetUserDataOnIO(
    content::ResourceContext* resource_context,
    scoped_refptr<InstantIOContext> instant_io_context) {
  resource_context->SetUserData(
      InstantIOContext::kInstantIOContextKeyName,
      new base::UserDataAdapter<InstantIOContext>(instant_io_context));
}

// static
void InstantIOContext::AddInstantProcessOnIO(
    scoped_refptr<InstantIOContext> instant_io_context, int process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  instant_io_context->process_ids_.insert(process_id);
}

// static
void InstantIOContext::RemoveInstantProcessOnIO(
    scoped_refptr<InstantIOContext> instant_io_context, int process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  instant_io_context->process_ids_.erase(process_id);
}

// static
void InstantIOContext::ClearInstantProcessesOnIO(
    scoped_refptr<InstantIOContext> instant_io_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  instant_io_context->process_ids_.clear();
}

// static
void InstantIOContext::AddMostVisitedItemsOnIO(
    scoped_refptr<InstantIOContext> instant_io_context,
    std::vector<InstantMostVisitedItemIDPair> items) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  instant_io_context->most_visited_item_cache_.AddItemsWithRestrictedID(items);
}


// static
bool InstantIOContext::ShouldServiceRequest(const net::URLRequest* request) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!info)
    return false;

  InstantIOContext* instant_io_context = GetDataForRequest(request);
  if (!instant_io_context)
    return false;

  int process_id = -1;
  int render_view_id = -1;
  if (info->GetAssociatedRenderView(&process_id, &render_view_id) &&
      instant_io_context->IsInstantProcess(process_id))
    return true;
  return false;
}

// static
bool InstantIOContext::GetURLForMostVisitedItemID(
    const net::URLRequest* request,
    InstantRestrictedID most_visited_item_id,
    GURL* url) {
  InstantIOContext* instant_io_context = GetDataForRequest(request);
  if (!instant_io_context) {
    *url = GURL();
    return false;
  }

  return instant_io_context->GetURLForMostVisitedItemID(most_visited_item_id,
                                                        url);
}

bool InstantIOContext::IsInstantProcess(int process_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return process_ids_.count(process_id) != 0;
}

bool InstantIOContext::GetURLForMostVisitedItemID(
    InstantRestrictedID most_visited_item_id,
    GURL* url) const {
  InstantMostVisitedItem item;
  if (most_visited_item_cache_.GetItemWithRestrictedID(most_visited_item_id,
                                                       &item)) {
    *url = item.url;
    return true;
  }

  *url = GURL();
  return false;
}
