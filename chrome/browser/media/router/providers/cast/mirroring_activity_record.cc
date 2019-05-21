// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/mirroring_activity_record.h"

#include <utility>

#include "chrome/common/media_router/mojo/media_router.mojom.h"
#include "components/mirroring/mojom/session_parameters.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"

using mirroring::mojom::CastMessageChannelPtr;
using mirroring::mojom::CastMessagePtr;
using mirroring::mojom::SessionError;
using mirroring::mojom::SessionObserverPtr;
using mirroring::mojom::SessionParameters;

namespace media_router {

MirroringActivityRecord::MirroringActivityRecord(
    const MediaRoute& route,
    const std::string& app_id,
    int32_t target_tab_id,
    mojom::MediaRouter* media_router)
    : ActivityRecord(route, app_id),
      observer_binding_(this),
      channel_binding_(this) {
  media_router->GetMirroringServiceHostForTab(target_tab_id,
                                              mojo::MakeRequest(&host_));
  SessionObserverPtr observer_ptr_;
  observer_binding_.Bind(mojo::MakeRequest(&observer_ptr_));

  CastMessageChannelPtr channel_ptr_;
  channel_binding_.Bind(mojo::MakeRequest(&channel_ptr_));

  // TODO(jrw): Set session parameters.
  host_->Start(SessionParameters::New(), std::move(observer_ptr_),
               std::move(channel_ptr_), mojo::MakeRequest(&inbound_channel_));
}

MirroringActivityRecord::~MirroringActivityRecord() = default;

void MirroringActivityRecord::OnError(SessionError error) {
  // TODO(jrw)
  NOTIMPLEMENTED();
}

void MirroringActivityRecord::DidStart() {
  // TODO(jrw)
  NOTIMPLEMENTED();
}

void MirroringActivityRecord::DidStop() {
  // TODO(jrw)
  NOTIMPLEMENTED();
}

void MirroringActivityRecord::Send(CastMessagePtr message) {
  // TODO(jrw)
  NOTIMPLEMENTED();
}

}  // namespace media_router
