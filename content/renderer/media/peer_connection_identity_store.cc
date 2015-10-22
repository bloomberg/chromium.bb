// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/peer_connection_identity_store.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/thread_task_runner_handle.h"
#include "content/renderer/media/webrtc_identity_service.h"
#include "content/renderer/render_thread_impl.h"

namespace content {
namespace {

const char kIdentityName[] = "WebRTC";

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
                url, first_party_for_cookies, kIdentityName, kIdentityName,
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

// Helper function for PeerConnectionIdentityStore::RequestIdentity.
// Used to invoke |observer|->OnSuccess in a PostTask.
void ObserverOnSuccess(
    const rtc::scoped_refptr<webrtc::DtlsIdentityRequestObserver>& observer,
    scoped_ptr<rtc::SSLIdentity> identity) {
  rtc::scoped_ptr<rtc::SSLIdentity> rtc_scoped_ptr(identity.release());
  observer->OnSuccess(rtc_scoped_ptr.Pass());
}

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

  // TODO(torbjorng): crbug.com/544902. Update store to use rtc::KeyParams.
  // With parameters such as modulesize, we cannot just call into the Chromium
  // code for some parameters (e.g. modulesize=1024, publicexponent=0x10001)
  // with the assumption that those are the parameters being used. I'd prefer to
  // never use Chromium's own code here, or else export its RSA parameters to a
  // header file so that we can invoke it only for exactly the parameters
  // requested here.
  if (key_type == rtc::KT_RSA) {
    // Use Chromium identity generation code for RSA. This generation code is
    // preferred over WebRTC RSA generation code for performance reasons.
    scoped_refptr<RequestHandler> handler(new RequestHandler(observer));
    main_thread_->PostTask(
        FROM_HERE,
        base::Bind(&RequestHandler::RequestIdentityOnUIThread, handler, url_,
                   first_party_for_cookies_));
  } else {
    // Use WebRTC identity generation code for non-RSA.
    scoped_ptr<rtc::SSLIdentity> identity(rtc::SSLIdentity::Generate(
        kIdentityName, key_type));

    scoped_refptr<base::SingleThreadTaskRunner> signaling_thread =
        base::ThreadTaskRunnerHandle::Get();

    // Invoke |observer| callbacks asynchronously. The callbacks of
    // DtlsIdentityStoreInterface implementations have to be async.
    if (identity) {
      // Async call to |observer|->OnSuccess.
      // Helper function necessary because OnSuccess takes an rtc::scoped_ptr
      // argument which has to be Pass()-ed. base::Passed gets around this for
      // scoped_ptr (without rtc namespace), but not for rtc::scoped_ptr.
      signaling_thread->PostTask(FROM_HERE,
          base::Bind(&ObserverOnSuccess, observer, base::Passed(&identity)));
    } else {
      // Async call to |observer|->OnFailure.
      signaling_thread->PostTask(FROM_HERE,
          base::Bind(&webrtc::DtlsIdentityRequestObserver::OnFailure,
                     observer, 0));
    }
  }
}

}  // namespace content
