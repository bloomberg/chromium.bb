// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_SESSION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_SESSION_SERVICE_IMPL_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/public/platform/modules/mediasession/media_session.mojom.h"

namespace content {

class RenderFrameHost;

class MediaSessionServiceImpl : public blink::mojom::MediaSessionService {
 public:
  ~MediaSessionServiceImpl() override;

  static void Create(RenderFrameHost* render_frame_host,
                     blink::mojom::MediaSessionServiceRequest request);

 private:
  explicit MediaSessionServiceImpl(RenderFrameHost* render_frame_host);

  void SetMetadata(
      const base::Optional<content::MediaMetadata>& metadata) override;

  void Bind(blink::mojom::MediaSessionServiceRequest request);

  RenderFrameHost* render_frame_host_;

  // RAII binding of |this| to an MediaSession interface request.
  // The binding is removed when binding_ is cleared or goes out of scope.
  std::unique_ptr<mojo::Binding<blink::mojom::MediaSessionService>> binding_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_SESSION_SERVICE_IMPL_H_
