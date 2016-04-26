// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/sampling/task_manager_io_thread_helper.h"

#include "chrome/browser/task_management/sampling/task_manager_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"

namespace task_management {

namespace {

TaskManagerIoThreadHelper* g_io_thread_helper = nullptr;

}  // namespace

IoThreadHelperManager::IoThreadHelperManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&TaskManagerIoThreadHelper::CreateInstance));
}

IoThreadHelperManager::~IoThreadHelperManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&TaskManagerIoThreadHelper::DeleteInstance));
}

// static
void TaskManagerIoThreadHelper::CreateInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!g_io_thread_helper);

  g_io_thread_helper = new TaskManagerIoThreadHelper;
}

// static
void TaskManagerIoThreadHelper::DeleteInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  delete g_io_thread_helper;
  g_io_thread_helper = nullptr;
}

// static
void TaskManagerIoThreadHelper::OnRawBytesRead(const net::URLRequest& request,
                                               int64_t bytes_read) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (g_io_thread_helper)
    g_io_thread_helper->OnNetworkBytesRead(request, bytes_read);
}

TaskManagerIoThreadHelper::TaskManagerIoThreadHelper() : weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

TaskManagerIoThreadHelper::~TaskManagerIoThreadHelper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void TaskManagerIoThreadHelper::OnMultipleBytesReadIO() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  DCHECK(!bytes_read_buffer_.empty());

  std::vector<BytesReadParam>* bytes_read_buffer =
      new std::vector<BytesReadParam>();
  bytes_read_buffer_.swap(*bytes_read_buffer);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&TaskManagerImpl::OnMultipleBytesReadUI,
                 base::Owned(bytes_read_buffer)));
}

void TaskManagerIoThreadHelper::OnNetworkBytesRead(
    const net::URLRequest& request, int64_t bytes_read) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Only net::URLRequestJob instances created by the ResourceDispatcherHost
  // have an associated ResourceRequestInfo and a render frame associated.
  // All other jobs will have -1 returned for the render process child and
  // routing ids - the jobs may still match a resource based on their origin id,
  // otherwise BytesRead() will attribute the activity to the Browser resource.
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(&request);
  int child_id = -1;
  int route_id = -1;
  if (info)
    info->GetAssociatedRenderFrame(&child_id, &route_id);

  // Get the origin PID of the request's originator.  This will only be set for
  // plugins - for renderer or browser initiated requests it will be zero.
  int origin_pid = info ? info->GetOriginPID() : 0;

  if (bytes_read_buffer_.empty()) {
    // Schedule a task to process the received bytes requests a second from now.
    // We're trying to calculate the tasks' network usage speed as bytes per
    // second so we collect as many requests during one seconds before the below
    // delayed TaskManagerIoThreadHelper::OnMultipleBytesReadIO() process them
    // after one second from now.
    content::BrowserThread::PostDelayedTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&TaskManagerIoThreadHelper::OnMultipleBytesReadIO,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(1));
  }

  bytes_read_buffer_.push_back(
      BytesReadParam(origin_pid, child_id, route_id, bytes_read));
}

}  // namespace task_management
