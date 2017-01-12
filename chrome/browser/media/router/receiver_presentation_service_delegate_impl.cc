// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/receiver_presentation_service_delegate_impl.h"

#include "chrome/browser/media/router/offscreen_presentation_manager.h"
#include "chrome/browser/media/router/offscreen_presentation_manager_factory.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    media_router::ReceiverPresentationServiceDelegateImpl);

using content::RenderFrameHost;

namespace media_router {

// static
void ReceiverPresentationServiceDelegateImpl::CreateForWebContents(
    content::WebContents* web_contents,
    const std::string& presentation_id) {
  DCHECK(web_contents);

  if (FromWebContents(web_contents))
    return;

  web_contents->SetUserData(UserDataKey(),
                            new ReceiverPresentationServiceDelegateImpl(
                                web_contents, presentation_id));
}

void ReceiverPresentationServiceDelegateImpl::AddObserver(
    int render_process_id,
    int render_frame_id,
    content::PresentationServiceDelegate::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(render_process_id, render_frame_id, observer);
}

void ReceiverPresentationServiceDelegateImpl::RemoveObserver(
    int render_process_id,
    int render_frame_id) {
  observers_.RemoveObserver(render_process_id, render_frame_id);
}

void ReceiverPresentationServiceDelegateImpl::Reset(int render_process_id,
                                                    int render_frame_id) {
  DVLOG(2) << __FUNCTION__ << render_process_id << ", " << render_frame_id;
  offscreen_presentation_manager_->OnOffscreenPresentationReceiverTerminated(
      presentation_id_);
}

ReceiverPresentationServiceDelegateImpl::
    ReceiverPresentationServiceDelegateImpl(content::WebContents* web_contents,
                                            const std::string& presentation_id)
    : web_contents_(web_contents),
      presentation_id_(presentation_id),
      offscreen_presentation_manager_(
          OffscreenPresentationManagerFactory::GetOrCreateForWebContents(
              web_contents_)) {
  DCHECK(web_contents_);
  DCHECK(!presentation_id.empty());
  DCHECK(offscreen_presentation_manager_);
}

void ReceiverPresentationServiceDelegateImpl::
    RegisterReceiverConnectionAvailableCallback(
        const content::ReceiverConnectionAvailableCallback&
            receiver_available_callback) {
  offscreen_presentation_manager_->OnOffscreenPresentationReceiverCreated(
      presentation_id_, web_contents_->GetLastCommittedURL(),
      receiver_available_callback);
}

}  // namespace media_router
