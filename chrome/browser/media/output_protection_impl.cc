// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/output_protection_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/media/output_protection_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

// static
void OutputProtectionImpl::Create(
    content::RenderFrameHost* render_frame_host,
    media::mojom::OutputProtectionRequest request) {
  DVLOG(2) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_frame_host);
  mojo::MakeStrongBinding(base::MakeUnique<OutputProtectionImpl>(
                              render_frame_host->GetProcess()->GetID(),
                              render_frame_host->GetRoutingID()),
                          std::move(request));
}

OutputProtectionImpl::OutputProtectionImpl(int render_process_id,
                                           int render_frame_id)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

OutputProtectionImpl::~OutputProtectionImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void OutputProtectionImpl::QueryStatus(const QueryStatusCallback& callback) {
  DVLOG(2) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GetProxy()->QueryStatus(base::Bind(&OutputProtectionImpl::OnQueryStatusResult,
                                     weak_factory_.GetWeakPtr(), callback));
}

void OutputProtectionImpl::EnableProtection(
    uint32_t desired_protection_mask,
    const EnableProtectionCallback& callback) {
  DVLOG(2) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GetProxy()->EnableProtection(
      desired_protection_mask,
      base::Bind(&OutputProtectionImpl::OnEnableProtectionResult,
                 weak_factory_.GetWeakPtr(), callback));
}

void OutputProtectionImpl::OnQueryStatusResult(
    const QueryStatusCallback& callback,
    bool success,
    uint32_t link_mask,
    uint32_t protection_mask) {
  DVLOG(2) << __func__ << ": success=" << success << ", link_mask=" << link_mask
           << ", protection_mask=" << protection_mask;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callback.Run(success, link_mask, protection_mask);
}

void OutputProtectionImpl::OnEnableProtectionResult(
    const EnableProtectionCallback& callback,
    bool success) {
  DVLOG(2) << __func__ << ": success=" << success;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callback.Run(success);
}

// Helper function to lazily create the |proxy_| and return it.
chrome::OutputProtectionProxy* OutputProtectionImpl::GetProxy() {
  if (!proxy_) {
    proxy_ = base::MakeUnique<chrome::OutputProtectionProxy>(render_process_id_,
                                                             render_frame_id_);
  }

  return proxy_.get();
}
