// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_SERVICE_IMPL_H_
#define CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_SERVICE_IMPL_H_

#include "base/containers/unique_ptr_adapters.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_service_base.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom.h"

namespace content {

class PictureInPictureSession;

// Receives Picture-in-Picture messages from a given RenderFrame. There is one
// PictureInPictureServiceImpl per RenderFrameHost. The service gets a hold of
// a PictureInPictureSession to which it delegates most of the interactions with
// the rest of the Picture-in-Picture classes such as
// PictureInPictureWindowController.
class CONTENT_EXPORT PictureInPictureServiceImpl final
    : public content::FrameServiceBase<blink::mojom::PictureInPictureService> {
 public:
  static void Create(RenderFrameHost*,
                     blink::mojom::PictureInPictureServiceRequest);
  static PictureInPictureServiceImpl* CreateForTesting(
      RenderFrameHost*,
      blink::mojom::PictureInPictureServiceRequest);

  // PictureInPictureService implementation.
  void StartSession(uint32_t player_id,
                    const base::Optional<viz::SurfaceId>& surface_id,
                    const gfx::Size& natural_size,
                    bool show_play_pause_button,
                    bool show_mute_button,
                    blink::mojom::PictureInPictureSessionObserverPtr,
                    StartSessionCallback) final;

  PictureInPictureSession* active_session_for_testing() const {
    return active_session_.get();
  }

 private:
  friend class PictureInPictureSession;

  PictureInPictureServiceImpl(RenderFrameHost*,
                              blink::mojom::PictureInPictureServiceRequest);
  ~PictureInPictureServiceImpl() override;

  RenderFrameHost* render_frame_host_ = nullptr;

  std::unique_ptr<PictureInPictureSession> active_session_;

  DISALLOW_COPY_AND_ASSIGN(PictureInPictureServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_SERVICE_IMPL_H_
