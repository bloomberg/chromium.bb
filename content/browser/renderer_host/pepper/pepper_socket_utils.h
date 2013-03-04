// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SOCKET_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SOCKET_UTILS_H_

#include "base/callback.h"
#include "base/logging.h"
#include "base/tracked_objects.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"
#include "content/public/common/socket_permission_request.h"
#include "ppapi/c/pp_instance.h"

struct PP_NetAddress_Private;

namespace content {

class RenderViewHost;

namespace pepper_socket_utils {

SocketPermissionRequest CreateSocketPermissionRequest(
    SocketPermissionRequest::OperationType type,
    const PP_NetAddress_Private& net_addr);

bool CanUseSocketAPIs(ProcessType plugin_process_type,
                      const SocketPermissionRequest& params,
                      RenderViewHost* render_view_host);

// Backend for PostOnUIThreadWithRenderViewHostAndReply. This converts the IDs
// to a RenderViewHost and calls the function, or returns a
// default-constructed return value on error.
template<typename ReturnType>
ReturnType CallWithRenderViewHost(
    int render_process_id,
    int render_view_id,
    const base::Callback<ReturnType(RenderViewHost*)>& task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RenderViewHost* render_view_host = RenderViewHost::FromID(render_process_id,
                                                            render_view_id);
  if (render_view_host)
    return task.Run(render_view_host);
  return ReturnType();
}

// Schedules the given callback to execute on the UI thread of the browser,
// passing the RenderViewHost associated with the given instance as a
// parameter.
//
// Normally this would be called from a ResourceHost with the reply using
// a weak pointer to itself.
//
// Importantly, the task will not be run if the RenderView is destroyed by
// the time we get to the UI thread, or if the PP_Instance is invalid, but
// the reply will still be run. The return type in this case will be a
// default-constructed |ReturnType|.
//
// So you may want to make sure you don't do silly things in the reply
// handler if the task on the UI thread is never run and you get a
// default-constructed result.
template<typename ReturnType>
bool PostOnUIThreadWithRenderViewHostAndReply(
    const BrowserPpapiHost* browser_ppapi_host,
    const tracked_objects::Location& from_here,
    PP_Instance instance,
    const base::Callback<ReturnType(RenderViewHost*)>& task,
    const base::Callback<void(ReturnType)>& reply) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  int render_process_id, render_view_id;
  bool success = browser_ppapi_host->GetRenderViewIDsForInstance(
      instance,
      &render_process_id,
      &render_view_id);
  if (!success)
    return false;
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI,
      from_here,
      base::Bind(&CallWithRenderViewHost<ReturnType>,
                 render_process_id, render_view_id, task),
      reply);
  return true;
}

}  // namespace pepper_socket_utils

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SOCKET_UTILS_H_
