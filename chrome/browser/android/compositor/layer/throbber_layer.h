// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_THROBBER_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_THROBBER_LAYER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/android/compositor/layer/layer.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {
class Layer;
class UIResourceLayer;
}

namespace chrome {
namespace android {

// A layer representing throbber which we use as a reloading spinner on the
// tab strip for tablets.
// TODO(changwan): implement WAITING state for which we should spin
// counterclockwise until connection to the server is made.
class ThrobberLayer : public Layer {
 public:
  static scoped_refptr<ThrobberLayer> Create(SkColor color);

  void Show(const base::Time& time);
  void Hide();
  void SetColor(SkColor color);
  void UpdateThrobber(const base::Time& time);

  // Layer:
  scoped_refptr<cc::Layer> layer() override;

 protected:
  explicit ThrobberLayer(unsigned int color);
  ~ThrobberLayer() override;

 private:
  SkColor color_;
  base::Time start_time_;
  scoped_refptr<cc::UIResourceLayer> layer_;
  scoped_refptr<cc::Layer> debug_layer_;

  DISALLOW_COPY_AND_ASSIGN(ThrobberLayer);
};

}  //  namespace android
}  //  namespace chrome

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_THROBBER_LAYER_H_
