// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TOP_CONTROLS_MANAGER_CLIENT_H_
#define CC_TOP_CONTROLS_MANAGER_CLIENT_H_

namespace cc {

class LayerTreeImpl;

class CC_EXPORT TopControlsManagerClient {
 public:
  virtual void setNeedsRedraw() = 0;
  virtual void setActiveTreeNeedsUpdateDrawProperties() = 0;
  virtual bool haveRootScrollLayer() const = 0;
  virtual float rootScrollLayerTotalScrollY() const = 0;

 protected:
  virtual ~TopControlsManagerClient() {}
};

}  // namespace cc

#endif  // CC_TOP_CONTROLS_MANAGER_CLIENT_H_
