// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/peer_connection_identity_store.h"

#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "content/renderer/media/webrtc_identity_service.h"
#include "content/renderer/render_thread_impl.h"

namespace content {
namespace {
// Bridges identity requests between the main render thread and libjingle's
// signaling thread.
class RequestHandler : public base::RefCountedThreadSafe<RequestHandler> {
 public:
  explicit RequestHandler(webrtc::DtlsIdentityRequestObserver* observer)
      : signaling_thread_(base::ThreadTaskRunnerHandle::Get()),
        observer_(observer) {}

  void RequestIdentityOnUIThread(const GURL& url,
                                 const GURL& first_party_for_cookies) {
    int request_id =
        RenderThreadImpl::current()
            ->get_webrtc_identity_service()
            ->RequestIdentity(
                url, first_party_for_cookies, "WebRTC", "WebRTC",
                base::Bind(&RequestHandler::OnIdentityReady, this),
                base::Bind(&RequestHandler::OnRequestFailed, this));
    DCHECK_NE(request_id, 0);
  }

 private:
  friend class base::RefCountedThreadSafe<RequestHandler>;
  ~RequestHandler() {
    DCHECK(!observer_.get());
  }

  void OnIdentityReady(
      const std::string& certificate,
      const std::string& private_key) {
    signaling_thread_->PostTask(FROM_HERE,
        base::Bind(static_cast<void (webrtc::DtlsIdentityRequestObserver::*)(
                       const std::string&, const std::string&)>(
                           &webrtc::DtlsIdentityRequestObserver::OnSuccess),
                   observer_, certificate, private_key));
    signaling_thread_->PostTask(FROM_HERE,
        base::Bind(&RequestHandler::EnsureReleaseObserverOnSignalingThread,
            this));
  }

  void OnRequestFailed(int error) {
    signaling_thread_->PostTask(FROM_HERE,
        base::Bind(&webrtc::DtlsIdentityRequestObserver::OnFailure, observer_,
                   error));
    signaling_thread_->PostTask(FROM_HERE,
        base::Bind(&RequestHandler::EnsureReleaseObserverOnSignalingThread,
            this));
  }

  void EnsureReleaseObserverOnSignalingThread() {
    DCHECK(signaling_thread_->BelongsToCurrentThread());
    observer_ = nullptr;
  }

  const scoped_refptr<base::SingleThreadTaskRunner> signaling_thread_;
  scoped_refptr<webrtc::DtlsIdentityRequestObserver> observer_;
};
}  // namespace

PeerConnectionIdentityStore::PeerConnectionIdentityStore(
    const GURL& url,
    const GURL& first_party_for_cookies)
    : main_thread_(base::ThreadTaskRunnerHandle::Get()),
      url_(url),
      first_party_for_cookies_(first_party_for_cookies) {
  signaling_thread_.DetachFromThread();
  DCHECK(main_thread_.get());
}

PeerConnectionIdentityStore::~PeerConnectionIdentityStore() {
  // Typically destructed on libjingle's signaling thread.
}

void PeerConnectionIdentityStore::RequestIdentity(
    rtc::KeyType key_type,
    const rtc::scoped_refptr<webrtc::DtlsIdentityRequestObserver>& observer) {
  DCHECK(signaling_thread_.CalledOnValidThread());
  DCHECK(observer);
  // This store only supports RSA.
  DCHECK_EQ(key_type, rtc::KT_RSA);

  scoped_refptr<RequestHandler> handler(new RequestHandler(observer));
  main_thread_->PostTask(
      FROM_HERE,
      base::Bind(&RequestHandler::RequestIdentityOnUIThread, handler, url_,
                 first_party_for_cookies_));
}

}  // namespace content
