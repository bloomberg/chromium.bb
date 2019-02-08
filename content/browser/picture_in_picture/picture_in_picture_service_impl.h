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

class PictureInPictureWindowControllerImpl;

// Receives Picture-in-Picture messages from a given RenderFrame. There is one
// PictureInPictureServiceImpl per RenderFrameHost. It talks directly to the
// PictureInPictureWindowControllerImpl. Only one service interacts with the
// window at a given time.
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
                    StartSessionCallback) final;
  void EndSession(EndSessionCallback) final;
  void UpdateSession(uint32_t player_id,
                     const base::Optional<viz::SurfaceId>& surface_id,
                     const gfx::Size& natural_size,
                     bool show_play_pause_button) final;
  void SetDelegate(blink::mojom::PictureInPictureDelegatePtr) final;

  void NotifyWindowResized(const gfx::Size&);

  // Returns the player that is currently in Picture-in-Picture in the context
  // of the frame associated with the service. Returns nullopt if there are
  // none.
  const base::Optional<MediaPlayerId>& player_id() const { return player_id_; }
  void ResetPlayerId() { player_id_.reset(); }

 private:
  PictureInPictureServiceImpl(RenderFrameHost*,
                              blink::mojom::PictureInPictureServiceRequest);
  ~PictureInPictureServiceImpl() override;

  // Returns the PictureInPictureWindowControllerImpl associated with the
  // WebContents. Can be null.
  PictureInPictureWindowControllerImpl* GetController();

  // Callack run when the delegate is disconnected. Only one delegate should be
  // set at any given time.
  void OnDelegateDisconnected();

  // Implementation of ExitPictureInPicture without callback handling.
  void ExitPictureInPictureInternal();

  WebContentsImpl* web_contents_impl();

  blink::mojom::PictureInPictureDelegatePtr delegate_ = nullptr;
  RenderFrameHost* render_frame_host_ = nullptr;
  base::Optional<MediaPlayerId> player_id_;

  DISALLOW_COPY_AND_ASSIGN(PictureInPictureServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_SERVICE_IMPL_H_
