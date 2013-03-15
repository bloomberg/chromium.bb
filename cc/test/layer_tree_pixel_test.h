// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "cc/solid_color_layer.h"
#include "cc/test/layer_tree_test_common.h"

#ifndef CC_TEST_LAYER_TREE_PIXEL_TEST_H_
#define CC_TEST_LAYER_TREE_PIXEL_TEST_H_

namespace cc {
class LayerTreeHost;

class LayerTreePixelTest : public ThreadedTest {
 protected:
  LayerTreePixelTest();
  virtual ~LayerTreePixelTest();

  bool RunTest(scoped_refptr<Layer> root_layer,
               const base::FilePath::StringType& file_name);

  virtual scoped_ptr<OutputSurface> createOutputSurface() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForMainThread() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForCompositorThread() OVERRIDE;
  virtual void swapBuffersOnThread(LayerTreeHostImpl* host_impl,
                                   bool result) OVERRIDE;

  virtual void beginTest() OVERRIDE;
  virtual void setupTree() OVERRIDE;
  virtual void afterTest() OVERRIDE;

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
