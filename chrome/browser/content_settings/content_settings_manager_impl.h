// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_MANAGER_IMPL_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_MANAGER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "chrome/common/content_settings_manager.mojom.h"

namespace content {
class RenderProcessHost;
}  // namespace content

namespace content_settings {
class CookieSettings;
}  // namespace content_settings

namespace chrome {

class ContentSettingsManagerImpl : public mojom::ContentSettingsManager {
 public:
  ~ContentSettingsManagerImpl() override;

  static void Create(
      content::RenderProcessHost* render_process_host,
      mojo::PendingReceiver<mojom::ContentSettingsManager> receiver);

  // mojom::ContentSettingsManager methods:
  void Clone(
      mojo::PendingReceiver<mojom::ContentSettingsManager> receiver) override;
  void AllowStorageAccess(int32_t render_frame_id,
                          StorageType storage_type,
                          const url::Origin& origin,
                          const GURL& site_for_cookies,
                          const url::Origin& top_frame_origin,
                          base::OnceCallback<void(bool)> callback) override;
  void OnContentBlocked(int32_t render_frame_id,
                        ContentSettingsType type) override;

 private:
  explicit ContentSettingsManagerImpl(
      content::RenderProcessHost* render_process_host);
  explicit ContentSettingsManagerImpl(const ContentSettingsManagerImpl& other);

  // Use these IDs to hold a weak reference back to the RenderFrameHost.
  const int render_process_id_;

  // Used to look up storage permissions.
  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
};

}  // namespace chrome

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_MANAGER_IMPL_H_
