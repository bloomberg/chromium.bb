// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_io_context.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;

namespace {

const char kInstantIOContextKeyName[] = "instant_io_context";

// Retrieves the Instant data from the |context|'s named user-data.
InstantIOContext* GetDataForResourceContext(
    content::ResourceContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!context->GetUserData(kInstantIOContextKeyName))
    context->SetUserData(kInstantIOContextKeyName, new InstantIOContext());
  return static_cast<InstantIOContext*>(
      context->GetUserData(kInstantIOContextKeyName));
}

}  // namespace

InstantIOContext::InstantIOContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

InstantIOContext::~InstantIOContext() {
}

// static
void InstantIOContext::AddInstantProcessOnIO(
    content::ResourceContext* context, int process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GetDataForResourceContext(context)->process_ids_.insert(process_id);
}

// static
void InstantIOContext::RemoveInstantProcessOnIO(
    content::ResourceContext* context, int process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GetDataForResourceContext(context)->process_ids_.erase(process_id);
}

// static
void InstantIOContext::ClearInstantProcessesOnIO(
    content::ResourceContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GetDataForResourceContext(context)->process_ids_.clear();
}

// static
bool InstantIOContext::ShouldServiceRequest(const net::URLRequest* request) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!info)
    return false;

  int process_id = -1;
  int render_view_id = -1;
  if (info->GetAssociatedRenderView(&process_id, &render_view_id) &&
      GetDataForResourceContext(info->GetContext())->IsInstantProcess(
          process_id))
    return true;
  return false;
}

bool InstantIOContext::IsInstantProcess(int process_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return process_ids_.count(process_id) != 0;
}
