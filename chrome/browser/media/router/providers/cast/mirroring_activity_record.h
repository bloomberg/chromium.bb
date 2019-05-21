// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_RECORD_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_RECORD_H_

#include <string>

#include "chrome/browser/media/router/providers/cast/activity_record.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/mojo/media_router.mojom-forward.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/mojom/mirroring_service_host.mojom.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace media_router {

class MirroringActivityRecord : public ActivityRecord,
                                public mirroring::mojom::SessionObserver,
                                public mirroring::mojom::CastMessageChannel {
 public:
  MirroringActivityRecord(const MediaRoute& route,
                          const std::string& app_id,
                          int32_t target_tab_id,
                          mojom::MediaRouter* media_router);
  ~MirroringActivityRecord() override;

  // SessionObserver implementation
  void OnError(mirroring::mojom::SessionError error) override;
  void DidStart() override;
  void DidStop() override;

  // CastMessageChannel implementation
  void Send(mirroring::mojom::CastMessagePtr message) override;

 private:
  mirroring::mojom::MirroringServiceHostPtr host_;
  mirroring::mojom::CastMessageChannelPtr inbound_channel_;
  mojo::Binding<mirroring::mojom::SessionObserver> observer_binding_;
  mojo::Binding<mirroring::mojom::CastMessageChannel> channel_binding_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_RECORD_H_
