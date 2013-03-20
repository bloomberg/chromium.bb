// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/test/layer_tree_test.h"

#ifndef CC_TEST_LAYER_TREE_PIXEL_TEST_H_
#define CC_TEST_LAYER_TREE_PIXEL_TEST_H_

namespace cc {
class LayerTreeHost;

class LayerTreePixelTest : public LayerTreeTest {
 protected:
  LayerTreePixelTest();
  virtual ~LayerTreePixelTest();

  virtual scoped_ptr<OutputSurface> CreateOutputSurface() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForMainThread() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForCompositorThread() OVERRIDE;
  virtual void SwapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE;

  virtual void BeginTest() OVERRIDE;
  virtual void SetupTree() OVERRIDE;
  virtual void AfterTest() OVERRIDE;

  scoped_refptr<SolidColorLayer> CreateSolidColorLayer(gfx::Rect rect,
                                                       SkColor color);

  void RunPixelTest(scoped_refptr<Layer> content_root,
                    base::FilePath file_name);

  // Common CSS colors defined for tests to use.
  enum Colors {
    kCSSOrange = 0xffffa500,
    kCSSBrown = 0xffa52a2a,
    kCSSGreen = 0xff008000,
  };

 private:
  scoped_refptr<Layer> content_root_;
  base::FilePath ref_file_;
};

}  // namespace cc

#endif  // CC_TEST_LAYER_TREE_PIXEL_TEST_H_
