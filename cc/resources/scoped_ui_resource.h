// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_SCOPED_UI_RESOURCE_H_
#define CC_RESOURCES_SCOPED_UI_RESOURCE_H_

#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_client.h"
#include "ui/gfx/size.h"

namespace cc {

class LayerTreeHost;

class CC_EXPORT ScopedUIResource : public UIResourceClient {
 public:
  static scoped_ptr<ScopedUIResource> Create(
      LayerTreeHost* host,
      scoped_refptr<UIResourceBitmap> bitmap);
  virtual ~ScopedUIResource();

  virtual scoped_refptr<UIResourceBitmap> GetBitmap(
      UIResourceId uid,
      bool resource_lost) OVERRIDE;
  UIResourceId id() { return id_; }

 protected:
  ScopedUIResource(LayerTreeHost* host, scoped_refptr<UIResourceBitmap> bitmap);

  // An empty default contructor for testing.
  ScopedUIResource();

  scoped_refptr<UIResourceBitmap> bitmap_;
  LayerTreeHost* host_;
  UIResourceId id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedUIResource);
};

}  // namespace cc

#endif  // CC_RESOURCES_SCOPED_UI_RESOURCE_H_
