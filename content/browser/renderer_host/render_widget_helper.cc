// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/view_messages.h"

namespace content {
namespace {

typedef std::map<int, RenderWidgetHelper*> WidgetHelperMap;
base::LazyInstance<WidgetHelperMap>::DestructorAtExit g_widget_helpers =
    LAZY_INSTANCE_INITIALIZER;

void AddWidgetHelper(int render_process_id,
                     const scoped_refptr<RenderWidgetHelper>& widget_helper) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // We don't care if RenderWidgetHelpers overwrite an existing process_id. Just
  // want this to be up to date.
  g_widget_helpers.Get()[render_process_id] = widget_helper.get();
}

}  // namespace

RenderWidgetHelper::RenderWidgetHelper()
    : render_process_id_(-1),
      resource_dispatcher_host_(NULL) {
}

RenderWidgetHelper::~RenderWidgetHelper() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Delete this RWH from the map if it is found.
  WidgetHelperMap& widget_map = g_widget_helpers.Get();
  WidgetHelperMap::iterator it = widget_map.find(render_process_id_);
  if (it != widget_map.end() && it->second == this)
    widget_map.erase(it);
}

void RenderWidgetHelper::Init(
    int render_process_id,
    ResourceDispatcherHostImpl* resource_dispatcher_host) {
  render_process_id_ = render_process_id;
  resource_dispatcher_host_ = resource_dispatcher_host;

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(&AddWidgetHelper, render_process_id_,
                                         make_scoped_refptr(this)));
}

int RenderWidgetHelper::GetNextRoutingID() {
  return next_routing_id_.GetNext() + 1;
}

// static
RenderWidgetHelper* RenderWidgetHelper::FromProcessHostID(
    int render_process_host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  WidgetHelperMap::const_iterator ci = g_widget_helpers.Get().find(
      render_process_host_id);
  return (ci == g_widget_helpers.Get().end())? NULL : ci->second;
}

void RenderWidgetHelper::ResumeDeferredNavigation(
    const GlobalRequestID& request_id) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&RenderWidgetHelper::OnResumeDeferredNavigation, this,
                     request_id));
}

void RenderWidgetHelper::OnResumeDeferredNavigation(
    const GlobalRequestID& request_id) {
  resource_dispatcher_host_->ResumeDeferredNavigation(request_id);
}

void RenderWidgetHelper::CreateNewWidget(int opener_id,
                                         blink::WebPopupType popup_type,
                                         mojom::WidgetPtr widget,
                                         int* route_id) {
  *route_id = GetNextRoutingID();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&RenderWidgetHelper::OnCreateWidgetOnUI, this, opener_id,
                     *route_id, widget.PassInterface(), popup_type));
}

void RenderWidgetHelper::CreateNewFullscreenWidget(int opener_id,
                                                   mojom::WidgetPtr widget,
                                                   int* route_id) {
  *route_id = GetNextRoutingID();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(&RenderWidgetHelper::OnCreateFullscreenWidgetOnUI, this,
                     opener_id, *route_id, widget.PassInterface()));
}

void RenderWidgetHelper::OnCreateWidgetOnUI(int32_t opener_id,
                                            int32_t route_id,
                                            mojom::WidgetPtrInfo widget_info,
                                            blink::WebPopupType popup_type) {
  mojom::WidgetPtr widget;
  widget.Bind(std::move(widget_info));
  RenderViewHostImpl* host = RenderViewHostImpl::FromID(
      render_process_id_, opener_id);
  if (host)
    host->CreateNewWidget(route_id, std::move(widget), popup_type);
}

void RenderWidgetHelper::OnCreateFullscreenWidgetOnUI(
    int32_t opener_id,
    int32_t route_id,
    mojom::WidgetPtrInfo widget_info) {
  mojom::WidgetPtr widget;
  widget.Bind(std::move(widget_info));
  RenderViewHostImpl* host = RenderViewHostImpl::FromID(
      render_process_id_, opener_id);
  if (host)
    host->CreateNewFullscreenWidget(route_id, std::move(widget));
}

}  // namespace content
