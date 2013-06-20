// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/test/layer_tree_test.h"

#ifndef CC_TEST_LAYER_TREE_PIXEL_TEST_H_
#define CC_TEST_LAYER_TREE_PIXEL_TEST_H_

class SkBitmap;

namespace cc {
class CopyOutputRequest;
class CopyOutputResult;
class LayerTreeHost;
class PixelComparator;
class TextureMailbox;

class LayerTreePixelTest : public LayerTreeTest {
 protected:
  LayerTreePixelTest();
  virtual ~LayerTreePixelTest();

  virtual scoped_ptr<OutputSurface> CreateOutputSurface() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForMainThread() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProviderForCompositorThread() OVERRIDE;

  virtual scoped_ptr<CopyOutputRequest> CreateCopyOutputRequest();

  void ReadbackResult(scoped_ptr<CopyOutputResult> result);

  virtual void BeginTest() OVERRIDE;
  virtual void SetupTree() OVERRIDE;
  virtual void AfterTest() OVERRIDE;

  scoped_refptr<SolidColorLayer> CreateSolidColorLayer(gfx::Rect rect,
                                                       SkColor color);
  scoped_refptr<SolidColorLayer> CreateSolidColorLayerWithBorder(
      gfx::Rect rect,
      SkColor color,
      int border_width,
      SkColor border_color);

  enum PixelTestType {
    GL_WITH_DEFAULT,
    GL_WITH_BITMAP,
    SOFTWARE_WITH_DEFAULT,
    SOFTWARE_WITH_BITMAP
  };

  void RunPixelTest(PixelTestType type,
                    scoped_refptr<Layer> content_root,
                    base::FilePath file_name);

  void RunPixelTestWithReadbackTarget(PixelTestType type,
                                      scoped_refptr<Layer> content_root,
                                      Layer* target,
                                      base::FilePath file_name);

  scoped_ptr<SkBitmap> CopyTextureMailboxToBitmap(
      gfx::Size size,
      const TextureMailbox& texture_mailbox);

  // Common CSS colors defined for tests to use.
  enum Colors {
    kCSSOrange = 0xffffa500,
    kCSSBrown = 0xffa52a2a,
    kCSSGreen = 0xff008000,
  };

  scoped_ptr<PixelComparator> pixel_comparator_;

 protected:
  PixelTestType test_type_;
  scoped_refptr<Layer> content_root_;
  Layer* readback_target_;
  base::FilePath ref_file_;
  scoped_ptr<SkBitmap> result_bitmap_;
};

}  // namespace cc

#endif  // CC_TEST_LAYER_TREE_PIXEL_TEST_H_
