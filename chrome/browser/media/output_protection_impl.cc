// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/output_protection_impl.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"

using media::mojom::OutputProtection;

// static
void OutputProtectionImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<OutputProtection> request) {
  DVLOG(2) << __FUNCTION__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_frame_host);

  // The created object is strongly bound to (and owned by) the pipe.
  new OutputProtectionImpl(render_frame_host, std::move(request));
}

OutputProtectionImpl::OutputProtectionImpl(
    content::RenderFrameHost* render_frame_host,
    mojo::InterfaceRequest<OutputProtection> request)
    : binding_(this, std::move(request)),
      render_frame_host_(render_frame_host),
      weak_factory_(this) {
  DCHECK(render_frame_host_);
}

OutputProtectionImpl::~OutputProtectionImpl() {}

void OutputProtectionImpl::QueryStatus(const QueryStatusCallback& callback) {
  DVLOG(2) << __FUNCTION__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NOTIMPLEMENTED();

  callback.Run(false, 0, 0);
}

void OutputProtectionImpl::EnableProtection(
    uint32_t desired_protection_mask,
    const EnableProtectionCallback& callback) {
  DVLOG(2) << __FUNCTION__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NOTIMPLEMENTED();

  callback.Run(false);
}
