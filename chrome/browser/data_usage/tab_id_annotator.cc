// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_usage/tab_id_annotator.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/data_usage/tab_id_provider.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "components/data_usage/core/data_use.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using data_usage::DataUse;

namespace chrome_browser_data_usage {

namespace {

// Attempts to get the associated tab ID for a given render frame. Returns -1 if
// no associated tab was found.
int32_t GetTabIdForRenderFrame(int render_process_id, int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(sclittle): For prerendering tabs, investigate if it's possible to find
  // the original tab that initiated the prerender.
  return SessionTabHelper::IdForTab(content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(render_process_id, render_frame_id)));
}

// Annotates |data_use| with the given |tab_id|, then passes it to |callback|.
// This is done in a separate function instead of as a method on TabIdAnnotator
// so that an in-progress annotation can complete even if the TabIdAnnotator is
// destroyed. This doesn't make much of a difference for production code, but
// makes it easier to test the TabIdAnnotator.
void AnnotateDataUse(
    std::unique_ptr<DataUse> data_use,
    const data_usage::DataUseAnnotator::DataUseConsumerCallback& callback,
    int32_t tab_id) {
  DCHECK(data_use);
  data_use->tab_id = tab_id;
  callback.Run(std::move(data_use));
}

}  // namespace

TabIdAnnotator::TabIdAnnotator() {}
TabIdAnnotator::~TabIdAnnotator() {}

void TabIdAnnotator::Annotate(net::URLRequest* request,
                              std::unique_ptr<DataUse> data_use,
                              const DataUseConsumerCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(data_use);

  TabIdProvider* existing_tab_id_provider = reinterpret_cast<TabIdProvider*>(
      request->GetUserData(TabIdProvider::kUserDataKey));
  if (existing_tab_id_provider) {
    existing_tab_id_provider->ProvideTabId(
        base::Bind(&AnnotateDataUse, base::Passed(&data_use), callback));
    return;
  }

  int render_process_id = -1, render_frame_id = -1;
  if (!content::ResourceRequestInfo::GetRenderFrameForRequest(
          request, &render_process_id, &render_frame_id)) {
    // Run the callback immediately with a tab ID of -1 if the request has no
    // render frame.
    AnnotateDataUse(std::move(data_use), callback, -1 /* tab_id */);
    return;
  }

  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner =
      BrowserThread::GetTaskRunnerForThread(BrowserThread::UI);
  std::unique_ptr<TabIdProvider> tab_id_provider(new TabIdProvider(
      ui_thread_task_runner.get(), FROM_HERE,
      base::Bind(&GetTabIdForRenderFrame, render_process_id, render_frame_id)));
  tab_id_provider->ProvideTabId(
      base::Bind(&AnnotateDataUse, base::Passed(&data_use), callback));

  // |request| takes ownership of |tab_id_provider|.
  request->SetUserData(TabIdProvider::kUserDataKey, tab_id_provider.release());
}

}  // namespace chrome_browser_data_usage
