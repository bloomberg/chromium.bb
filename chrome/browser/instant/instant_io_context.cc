// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_io_context.h"

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

InstantIOContext::InstantIOContext() {
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
void InstantIOContext::AddMostVisitedItemIDOnIO(
    scoped_refptr<InstantIOContext> instant_io_context,
    uint64 most_visited_item_id, const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  instant_io_context->most_visited_item_id_to_url_map_[most_visited_item_id] =
      url;
}

// static
void InstantIOContext::DeleteMostVisitedURLsOnIO(
    scoped_refptr<InstantIOContext> instant_io_context,
    std::vector<uint64> deleted_ids, bool all_history) {
  if (all_history) {
    instant_io_context->most_visited_item_id_to_url_map_.clear();
    return;
  }

  for (size_t i = 0; i < deleted_ids.size(); ++i) {
    instant_io_context->most_visited_item_id_to_url_map_.erase(
        instant_io_context->most_visited_item_id_to_url_map_.find(
            deleted_ids[i]));
  }
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
bool InstantIOContext::GetURLForMostVisitedItemId(
    const net::URLRequest* request,
    uint64 most_visited_item_id,
    GURL* url) {
  InstantIOContext* instant_io_context = GetDataForRequest(request);
  if (!instant_io_context) {
    *url = GURL();
    return false;
  }

  return instant_io_context->GetURLForMostVisitedItemId(most_visited_item_id,
                                                        url);
}

bool InstantIOContext::IsInstantProcess(int process_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return process_ids_.count(process_id) != 0;
}

bool InstantIOContext::GetURLForMostVisitedItemId(uint64 most_visited_item_id,
                                                  GURL* url) {
  std::map<uint64, GURL>::iterator it =
      most_visited_item_id_to_url_map_.find(most_visited_item_id);
  if (it != most_visited_item_id_to_url_map_.end()) {
    *url = it->second;
    return true;
  }
  *url = GURL();
  return false;
}
