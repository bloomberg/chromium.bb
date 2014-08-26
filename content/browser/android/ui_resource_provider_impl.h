// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_UI_RESOURCE_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_ANDROID_UI_RESOURCE_PROVIDER_IMPL_H_

#include "base/containers/hash_tables.h"
#include "content/browser/android/system_ui_resource_manager_impl.h"
#include "content/public/browser/android/ui_resource_provider.h"

namespace cc {
class LayerTreeHost;
}

namespace content {

class UIResourceClientAndroid;

class UIResourceProviderImpl : public UIResourceProvider {
 public:
  UIResourceProviderImpl();

  virtual ~UIResourceProviderImpl();

  void SetLayerTreeHost(cc::LayerTreeHost* host);

  void UIResourcesAreInvalid();

  virtual cc::UIResourceId CreateUIResource(
      UIResourceClientAndroid* client) OVERRIDE;

  virtual void DeleteUIResource(cc::UIResourceId resource_id) OVERRIDE;

  ui::SystemUIResourceManager& GetSystemUIResourceManager();

  void SetSupportsETC1NonPowerOfTwo(bool supports_etc1_npot) {
    supports_etc1_npot_ = supports_etc1_npot;
  }
  virtual bool SupportsETC1NonPowerOfTwo() const OVERRIDE;

 private:
  typedef base::hash_map<cc::UIResourceId, UIResourceClientAndroid*>
      UIResourceClientMap;
  UIResourceClientMap ui_resource_client_map_;
  SystemUIResourceManagerImpl system_ui_resource_manager_;
  cc::LayerTreeHost* host_;
  bool supports_etc1_npot_;

  DISALLOW_COPY_AND_ASSIGN(UIResourceProviderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_UI_RESOURCE_PROVIDER_IMPL_H_
