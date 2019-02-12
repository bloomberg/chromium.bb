// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"

#include <utility>

#include "content/browser/picture_in_picture/picture_in_picture_window_controller_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

// static
void PictureInPictureServiceImpl::Create(
    RenderFrameHost* render_frame_host,
    blink::mojom::PictureInPictureServiceRequest request) {
  DCHECK(render_frame_host);
  new PictureInPictureServiceImpl(render_frame_host, std::move(request));
}

// static
PictureInPictureServiceImpl* PictureInPictureServiceImpl::CreateForTesting(
    RenderFrameHost* render_frame_host,
    blink::mojom::PictureInPictureServiceRequest request) {
  return new PictureInPictureServiceImpl(render_frame_host, std::move(request));
}

void PictureInPictureServiceImpl::StartSession(
    uint32_t player_id,
    const base::Optional<viz::SurfaceId>& surface_id,
    const gfx::Size& natural_size,
    bool show_play_pause_button,
    StartSessionCallback callback) {
  player_id_ = MediaPlayerId(render_frame_host_, player_id);

  auto* pip_controller = GetController();
  if (pip_controller)
    pip_controller->set_service(this);

  gfx::Size window_size = web_contents_impl()->EnterPictureInPicture(
      surface_id.value(), natural_size);

  if (pip_controller)
    pip_controller->SetAlwaysHidePlayPauseButton(show_play_pause_button);

  std::move(callback).Run(window_size);
}

void PictureInPictureServiceImpl::EndSession(EndSessionCallback callback) {
  DCHECK(player_id_);

  ExitPictureInPictureInternal();

  std::move(callback).Run();
}

void PictureInPictureServiceImpl::UpdateSession(
    uint32_t player_id,
    const base::Optional<viz::SurfaceId>& surface_id,
    const gfx::Size& natural_size,
    bool show_play_pause_button) {
  player_id_ = MediaPlayerId(render_frame_host_, player_id);

  // The PictureInPictureWindowController instance may not have been created by
  // the embedder.
  if (auto* pip_controller = GetController()) {
    pip_controller->EmbedSurface(surface_id.value(), natural_size);
    pip_controller->SetAlwaysHidePlayPauseButton(show_play_pause_button);
    pip_controller->set_service(this);
  }
}

void PictureInPictureServiceImpl::SetDelegate(
    blink::mojom::PictureInPictureDelegatePtr delegate) {
  delegate.set_connection_error_handler(
      base::BindOnce(&PictureInPictureServiceImpl::OnDelegateDisconnected,
                     // delegate is held by |this|.
                     base::Unretained(this)));

  if (delegate_)
    mojo::ReportBadMessage("SetDelegate() should only be called once.");

  delegate_ = std::move(delegate);
}

void PictureInPictureServiceImpl::NotifyWindowResized(const gfx::Size& size) {
  if (delegate_)
    delegate_->PictureInPictureWindowSizeChanged(size);
}

PictureInPictureServiceImpl::PictureInPictureServiceImpl(
    RenderFrameHost* render_frame_host,
    blink::mojom::PictureInPictureServiceRequest request)
    : FrameServiceBase(render_frame_host, std::move(request)),
      render_frame_host_(render_frame_host) {}

PictureInPictureServiceImpl::~PictureInPictureServiceImpl() {
  if (player_id_)
    ExitPictureInPictureInternal();
  if (GetController())
    GetController()->set_service(nullptr);
}

PictureInPictureWindowControllerImpl*
PictureInPictureServiceImpl::GetController() {
  return PictureInPictureWindowControllerImpl::GetOrCreateForWebContents(
      web_contents_impl());
}

void PictureInPictureServiceImpl::OnDelegateDisconnected() {
  delegate_ = nullptr;
}

void PictureInPictureServiceImpl::ExitPictureInPictureInternal() {
  web_contents_impl()->ExitPictureInPicture();

  // Reset must happen after notifying the WebContents because it may interact
  // with it.
  player_id_.reset();

  if (auto* controller = GetController())
    controller->set_service(nullptr);
}

WebContentsImpl* PictureInPictureServiceImpl::web_contents_impl() {
  return static_cast<WebContentsImpl*>(web_contents());
}

}  // namespace content
