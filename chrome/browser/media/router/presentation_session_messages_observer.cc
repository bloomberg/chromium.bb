// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation_session_messages_observer.h"

#include "chrome/browser/media/router/media_router.h"

namespace media_router {

PresentationSessionMessagesObserver::PresentationSessionMessagesObserver(
    const content::PresentationSessionMessageCallback& message_cb,
    const MediaRoute::Id& route_id,
    MediaRouter* router)
    : message_cb_(message_cb), route_id_(route_id), router_(router) {
  DCHECK(!message_cb_.is_null());
  DCHECK(!route_id_.empty());
  DCHECK(router_);
  router_->RegisterPresentationSessionMessagesObserver(this);
}

PresentationSessionMessagesObserver::~PresentationSessionMessagesObserver() {
  router_->UnregisterPresentationSessionMessagesObserver(this);
}

void PresentationSessionMessagesObserver::OnMessagesReceived(
    const ScopedVector<content::PresentationSessionMessage>& messages,
    bool pass_ownership) {
  DVLOG(2) << __FUNCTION__ << ", number of messages : " << messages.size()
           << " (pass_ownership = " << pass_ownership << ")";
  DCHECK(!messages.empty());
  message_cb_.Run(messages, pass_ownership);
}

}  // namespace media_router
