// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CDM_BROWSER_MEDIA_DRM_STORAGE_IMPL_H_
#define COMPONENTS_CDM_BROWSER_MEDIA_DRM_STORAGE_IMPL_H_

#include "base/threading/thread_checker.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/mojo/interfaces/media_drm_storage.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "url/origin.h"

class PrefRegistrySimple;
class PrefService;

namespace content {
class RenderFrameHost;
}

namespace cdm {

// Implements media::mojom::MediaDrmStorage using PrefService.
// This file is located under components/ so that it can be shared by multiple
// content embedders (e.g. chrome and chromecast).
class MediaDrmStorageImpl final : public media::mojom::MediaDrmStorage,
                                  public content::WebContentsObserver {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  MediaDrmStorageImpl(content::RenderFrameHost* render_frame_host,
                      PrefService* pref_service,
                      const url::Origin& origin,
                      media::mojom::MediaDrmStorageRequest request);
  ~MediaDrmStorageImpl() final;

  // media::mojom::MediaDrmStorage implementation.
  void Initialize(const url::Origin& origin) final;
  void OnProvisioned(OnProvisionedCallback callback) final;
  void SavePersistentSession(const std::string& session_id,
                             media::mojom::SessionDataPtr session_data,
                             SavePersistentSessionCallback callback) final;
  void LoadPersistentSession(const std::string& session_id,
                             LoadPersistentSessionCallback callback) final;
  void RemovePersistentSession(const std::string& session_id,
                               RemovePersistentSessionCallback callback) final;

  // content::WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) final;
  void DidFinishNavigation(content::NavigationHandle* navigation_handle) final;

 private:
  base::ThreadChecker thread_checker_;

  // Stops observing WebContents and delete |this|.
  void Close();

  content::RenderFrameHost* const render_frame_host_ = nullptr;
  PrefService* const pref_service_ = nullptr;
  const url::Origin origin_;
  const std::string origin_string_;
  bool initialized_ = false;

  mojo::Binding<media::mojom::MediaDrmStorage> binding_;
};

}  // namespace cdm

#endif  // COMPONENTS_CDM_BROWSER_MEDIA_DRM_STORAGE_IMPL_H_
