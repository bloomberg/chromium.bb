// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/peer_connection_identity_service.h"

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
  explicit RequestHandler(webrtc::DTLSIdentityRequestObserver* observer)
      : signaling_thread_(base::ThreadTaskRunnerHandle::Get()),
        observer_(observer) {}

  void RequestIdentityOnUIThread(const GURL& url,
                                 const GURL& first_party_for_cookies,
                                 const std::string& identity_name,
                                 const std::string& common_name) {
    int request_id =
        RenderThreadImpl::current()
            ->get_webrtc_identity_service()
            ->RequestIdentity(
                url, first_party_for_cookies, identity_name, common_name,
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
        base::Bind(&webrtc::DTLSIdentityRequestObserver::OnSuccess, observer_,
                   certificate, private_key));
    signaling_thread_->PostTask(FROM_HERE,
        base::Bind(&RequestHandler::EnsureReleaseObserverOnSignalingThread,
            this));
  }

  void OnRequestFailed(int error) {
    signaling_thread_->PostTask(FROM_HERE,
        base::Bind(&webrtc::DTLSIdentityRequestObserver::OnFailure, observer_,
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
  scoped_refptr<webrtc::DTLSIdentityRequestObserver> observer_;
};
}  // namespace

PeerConnectionIdentityService::PeerConnectionIdentityService(
    const GURL& url,
    const GURL& first_party_for_cookies)
    : main_thread_(base::ThreadTaskRunnerHandle::Get()),
      url_(url),
      first_party_for_cookies_(first_party_for_cookies) {
  signaling_thread_.DetachFromThread();
  DCHECK(main_thread_.get());
}

PeerConnectionIdentityService::~PeerConnectionIdentityService() {
  // Typically destructed on libjingle's signaling thread.
}

bool PeerConnectionIdentityService::RequestIdentity(
    const std::string& identity_name,
    const std::string& common_name,
    webrtc::DTLSIdentityRequestObserver* observer) {
  DCHECK(signaling_thread_.CalledOnValidThread());
  DCHECK(observer);

  scoped_refptr<RequestHandler> handler(new RequestHandler(observer));
  main_thread_->PostTask(
      FROM_HERE,
      base::Bind(&RequestHandler::RequestIdentityOnUIThread, handler, url_,
                 first_party_for_cookies_, identity_name, common_name));

  return true;
}

}  // namespace content
