// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_dtmf_sender_handler.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"

using webrtc::DtmfSenderInterface;

namespace content {

class RtcDtmfSenderHandler::Observer :
    public base::RefCountedThreadSafe<Observer>,
    public webrtc::DtmfSenderObserverInterface {
 public:
  explicit Observer(const base::WeakPtr<RtcDtmfSenderHandler>& handler)
      : main_thread_(base::ThreadTaskRunnerHandle::Get()), handler_(handler) {}

 private:
  friend class base::RefCountedThreadSafe<Observer>;

  ~Observer() override {}

  void OnToneChange(const std::string& tone) override {
    main_thread_->PostTask(FROM_HERE,
        base::Bind(&RtcDtmfSenderHandler::Observer::OnToneChangeOnMainThread,
                   this, tone));
  }

  void OnToneChangeOnMainThread(const std::string& tone) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (handler_)
      handler_->OnToneChange(tone);
  }

  base::ThreadChecker thread_checker_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  base::WeakPtr<RtcDtmfSenderHandler> handler_;
};

RtcDtmfSenderHandler::RtcDtmfSenderHandler(DtmfSenderInterface* dtmf_sender)
    : dtmf_sender_(dtmf_sender),
      webkit_client_(NULL),
      weak_factory_(this) {
  DVLOG(1) << "::ctor";
  observer_ = new Observer(weak_factory_.GetWeakPtr());
  dtmf_sender_->RegisterObserver(observer_.get());
}

RtcDtmfSenderHandler::~RtcDtmfSenderHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "::dtor";
  dtmf_sender_->UnregisterObserver();
  // Release |observer| before |weak_factory_| is destroyed.
  observer_ = NULL;
}

void RtcDtmfSenderHandler::SetClient(
    blink::WebRTCDTMFSenderHandlerClient* client) {
  webkit_client_ = client;
}

blink::WebString RtcDtmfSenderHandler::CurrentToneBuffer() {
  return blink::WebString::FromUTF8(dtmf_sender_->tones());
}

bool RtcDtmfSenderHandler::CanInsertDTMF() {
  return dtmf_sender_->CanInsertDtmf();
}

bool RtcDtmfSenderHandler::InsertDTMF(const blink::WebString& tones,
                                      long duration,
                                      long interToneGap) {
  std::string utf8_tones = tones.Utf8();
  return dtmf_sender_->InsertDtmf(utf8_tones, static_cast<int>(duration),
                                  static_cast<int>(interToneGap));
}

void RtcDtmfSenderHandler::OnToneChange(const std::string& tone) {
  if (!webkit_client_) {
    LOG(ERROR) << "WebRTCDTMFSenderHandlerClient not set.";
    return;
  }
  webkit_client_->DidPlayTone(blink::WebString::FromUTF8(tone));
}

}  // namespace content
