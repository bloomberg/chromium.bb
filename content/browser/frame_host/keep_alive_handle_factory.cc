// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/keep_alive_handle_factory.h"

#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"

namespace content {

namespace {
constexpr base::TimeDelta kDefaultTimeout = base::TimeDelta::FromSeconds(30);
}  // namespace

class KeepAliveHandleFactory::Context final : public base::RefCounted<Context> {
 public:
  explicit Context(RenderProcessHost* process_host)
      : process_id_(process_host->GetID()), weak_ptr_factory_(this) {
    DCHECK(!process_host->IsKeepAliveRefCountDisabled());
    process_host->IncrementKeepAliveRefCount();
  }

  void Detach() {
    if (detached_)
      return;
    detached_ = true;
    RenderProcessHost* process_host = RenderProcessHost::FromID(process_id_);
    if (!process_host || process_host->IsKeepAliveRefCountDisabled())
      return;

    process_host->DecrementKeepAliveRefCount();
  }

  void DetachLater() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto timeout = kDefaultTimeout;
    int timeout_set_by_finch_in_sec = base::GetFieldTrialParamByFeatureAsInt(
        features::kKeepAliveRendererForKeepaliveRequests, "timeout_in_sec",
        kDefaultTimeout.InSeconds());
    // Adopt only "reasonable" values.
    if (timeout_set_by_finch_in_sec > 0 && timeout_set_by_finch_in_sec <= 60) {
      timeout = base::TimeDelta::FromSeconds(timeout_set_by_finch_in_sec);
    } else {
      DVLOG(0) << "'timeout_in_sec' param for "
               << "'keep-alive-renderer-for-keepalive-requests' is ignored "
               << "due to out of range.";
    }
    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&Context::Detach, weak_ptr_factory_.GetWeakPtr()),
        timeout);
  }

  void AddBinding(std::unique_ptr<mojom::KeepAliveHandle> impl,
                  mojom::KeepAliveHandleRequest request) {
    binding_set_.AddBinding(std::move(impl), std::move(request));
  }

 private:
  friend class base::RefCounted<Context>;
  ~Context() { Detach(); }

  mojo::StrongBindingSet<mojom::KeepAliveHandle> binding_set_;
  const int process_id_;
  bool detached_ = false;

  base::WeakPtrFactory<Context> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

class KeepAliveHandleFactory::KeepAliveHandleImpl final
    : public mojom::KeepAliveHandle {
 public:
  explicit KeepAliveHandleImpl(scoped_refptr<Context> context)
      : context_(std::move(context)) {}
  ~KeepAliveHandleImpl() override {}

 private:
  scoped_refptr<Context> context_;

  DISALLOW_COPY_AND_ASSIGN(KeepAliveHandleImpl);
};

KeepAliveHandleFactory::KeepAliveHandleFactory(RenderProcessHost* process_host)
    : context_(base::MakeRefCounted<Context>(process_host)) {}

KeepAliveHandleFactory::~KeepAliveHandleFactory() {
  context_->DetachLater();
}

void KeepAliveHandleFactory::Create(mojom::KeepAliveHandleRequest request) {
  context_->AddBinding(std::make_unique<KeepAliveHandleImpl>(context_),
                       std::move(request));
}

}  // namespace content
