// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SYSTEM_UI_RESOURCE_MANAGER_IMPL_H_
#define CONTENT_BROWSER_ANDROID_SYSTEM_UI_RESOURCE_MANAGER_IMPL_H_

#include "base/basictypes.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ui/base/android/system_ui_resource_manager.h"

class SkBitmap;

namespace cc {
class UIResourceBitmap;
}

namespace content {

class UIResourceProvider;

class CONTENT_EXPORT SystemUIResourceManagerImpl
    : public ui::SystemUIResourceManager {
 public:
  explicit SystemUIResourceManagerImpl(
      UIResourceProvider* ui_resource_provider);
  virtual ~SystemUIResourceManagerImpl();

  virtual void PreloadResource(ResourceType type) OVERRIDE;
  virtual cc::UIResourceId GetUIResourceId(ResourceType type) OVERRIDE;

 private:
  friend class TestSystemUIResourceManagerImpl;
  class Entry;

  // Start loading the resource bitmap.  virtual for testing.
  virtual void BuildResource(ResourceType type);

  Entry* GetEntry(ResourceType type);
  void OnFinishedLoadBitmap(ResourceType, SkBitmap* bitmap_holder);

  scoped_ptr<Entry> resource_map_[RESOURCE_TYPE_LAST + 1];
  UIResourceProvider* ui_resource_provider_;

  base::WeakPtrFactory<SystemUIResourceManagerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemUIResourceManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_SYSTEM_UI_RESOURCE_MANAGER_IMPL_H_
