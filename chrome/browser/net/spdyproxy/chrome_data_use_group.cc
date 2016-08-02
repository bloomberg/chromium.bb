// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/chrome_data_use_group.h"

#include "base/bind.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request.h"

ChromeDataUseGroup::ChromeDataUseGroup(const net::URLRequest* request)
    : initialized_(false),
      initilized_on_ui_thread_(false),
      render_process_id_(-1),
      render_frame_id_(-1),
      has_valid_render_frame_id_(false),
      url_(request->url()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  has_valid_render_frame_id_ =
      content::ResourceRequestInfo::GetRenderFrameForRequest(
          request, &render_process_id_, &render_frame_id_);
}

ChromeDataUseGroup::~ChromeDataUseGroup() {}

std::string ChromeDataUseGroup::GetHostname() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!initilized_on_ui_thread_)
    InitializeOnUIThread();

  DCHECK(initilized_on_ui_thread_);

  return url_.HostNoBrackets();
}

void ChromeDataUseGroup::Initialize() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (initialized_)
    return;

  initialized_ = true;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&ChromeDataUseGroup::InitializeOnUIThread, this));
}

void ChromeDataUseGroup::InitializeOnUIThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (initilized_on_ui_thread_)
    return;

  initilized_on_ui_thread_ = true;

  if (!has_valid_render_frame_id_)
    return;

  content::WebContents* tab = content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_));
  if (tab)
    url_ = tab->GetVisibleURL();
}
