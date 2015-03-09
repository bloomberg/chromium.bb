// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TOOLBAR_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TOOLBAR_LAYER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/android/compositor/layer/layer.h"
#include "ui/android/resources/resource_manager.h"

namespace cc {
class Layer;
class SolidColorLayer;
class UIResourceLayer;
}

namespace chrome {
namespace android {

class ToolbarLayer : public Layer {
 public:
  static scoped_refptr<ToolbarLayer> Create();

  // Implements Layer
  scoped_refptr<cc::Layer> layer() override;

  void PushResource(ui::ResourceManager::Resource* resource,
                    ui::ResourceManager::Resource* progress_resource,
                    bool anonymize,
                    bool anonymize_component_is_incognito,
                    bool show_debug);

 protected:
  ToolbarLayer();
  ~ToolbarLayer() override;

 private:
  scoped_refptr<cc::Layer> layer_;
  scoped_refptr<cc::UIResourceLayer> bitmap_layer_;
  scoped_refptr<cc::UIResourceLayer> progress_layer_;
  scoped_refptr<cc::SolidColorLayer> anonymize_layer_;
  scoped_refptr<cc::SolidColorLayer> debug_layer_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarLayer);
};

}  //  namespace android
}  //  namespace chrome

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TOOLBAR_LAYER_H_
