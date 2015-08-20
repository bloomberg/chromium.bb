// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_SESSION_MESSAGES_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_SESSION_MESSAGES_OBSERVER_H_

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/media/router/media_route.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/browser/presentation_session.h"

namespace media_router {

class MediaRouter;

// Observes messages originating from the MediaSink connected to a MediaRoute
// that represents a presentation.
// Messages are received from MediaRouter via |OnMessagesReceived|, where
// |message_cb_| will be invoked.
class PresentationSessionMessagesObserver {
 public:
  // |message_cb|: The callback to invoke whenever messages are received.
  // |route_id|: ID of MediaRoute to listen for messages.
  PresentationSessionMessagesObserver(
      const content::PresentationSessionMessageCallback& message_cb,
      const MediaRoute::Id& route_id,
      MediaRouter* router);
  ~PresentationSessionMessagesObserver();

  // Invoked by |router_| whenever messages are received. Invokes |message_cb_|
  // with |messages|.
  // |messages| is guaranteed to be non-empty.
  // If |pass_ownership| is true, then this observer may reuse buffers from the
  // contents of |messages| without copying.
  void OnMessagesReceived(
      const ScopedVector<content::PresentationSessionMessage>& messages,
      bool pass_ownership);

  const MediaRoute::Id& route_id() const { return route_id_; }

 private:
  content::PresentationSessionMessageCallback message_cb_;
  const MediaRoute::Id route_id_;
  MediaRouter* const router_;

  DISALLOW_COPY_AND_ASSIGN(PresentationSessionMessagesObserver);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_SESSION_MESSAGES_OBSERVER_H_
