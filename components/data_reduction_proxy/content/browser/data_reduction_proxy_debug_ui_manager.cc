// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_debug_ui_manager.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace data_reduction_proxy {

namespace {
// Once the blocking page is shown, it is not displayed for another five
// minutes.
const int kBlockingPageDelayInMinutes = 5;
}

DataReductionProxyDebugUIManager::BypassResource::BypassResource()
    : is_subresource(false),
      render_process_host_id(-1),
      render_view_id(-1) {
}

DataReductionProxyDebugUIManager::BypassResource::~BypassResource() {
}

DataReductionProxyDebugUIManager::DataReductionProxyDebugUIManager(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : ui_task_runner_(ui_task_runner),
      io_task_runner_(io_task_runner) {
}

DataReductionProxyDebugUIManager::~DataReductionProxyDebugUIManager() {
}

bool DataReductionProxyDebugUIManager::IsTabClosed(
    const BypassResource& resource) const {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  // The tab might have been closed.
  content::RenderViewHost* render_view_host =
      content::RenderViewHost::FromID(resource.render_process_host_id,
                                      resource.render_view_id);
  content::WebContents* web_contents = nullptr;
  if (render_view_host)
    web_contents = content::WebContents::FromRenderViewHost(render_view_host);

  if(web_contents)
    return false;
  return true;
}

void DataReductionProxyDebugUIManager::DisplayBlockingPage(
    const BypassResource& resource) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  // Check if the user has already ignored the warning for this render_view
  // and domain.
  if (base::Time::Now() - blocking_page_last_shown_ <
          base::TimeDelta::FromMinutes(kBlockingPageDelayInMinutes)) {
    if (!resource.callback.is_null())
      io_task_runner_->PostTask(FROM_HERE, base::Bind(resource.callback, true));
    return;
  }

  if (IsTabClosed(resource)) {
    // The tab is gone and there was not a chance to show the interstitial.
    // Just act as if "Don't Proceed" were chosen.
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DataReductionProxyDebugUIManager::OnBlockingPageDone,
                   this, resource, false));
    return;
  }

  // TODO(megjablon): Show blocking page. For now, continue on to the page.
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyDebugUIManager::OnBlockingPageDone,
                 this, resource, true));
}

void DataReductionProxyDebugUIManager::OnBlockingPageDone(
    const BypassResource& resource,
    bool proceed) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!resource.callback.is_null())
    resource.callback.Run(proceed);
  if (proceed)
    blocking_page_last_shown_ = base::Time::Now();
}

}  // namespace data_reduction_proxy
