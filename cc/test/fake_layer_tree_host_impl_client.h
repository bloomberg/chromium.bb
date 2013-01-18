// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_CLIENT_H_
#define CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_CLIENT_H_

#include "cc/layer_tree_host_impl.h"

namespace cc {

class FakeLayerTreeHostImplClient : public LayerTreeHostImplClient {
 public:
  // LayerTreeHostImplClient implementation.
  virtual void didLoseOutputSurfaceOnImplThread() OVERRIDE { }
  virtual void onSwapBuffersCompleteOnImplThread() OVERRIDE { }
  virtual void onVSyncParametersChanged(
      base::TimeTicks,
      base::TimeDelta) OVERRIDE { }
  virtual void onCanDrawStateChanged(bool) OVERRIDE { }
  virtual void onHasPendingTreeStateChanged(bool) OVERRIDE { }
  virtual void setNeedsRedrawOnImplThread() OVERRIDE { }
  virtual void didSwapUseIncompleteTextureOnImplThread() OVERRIDE { }
  virtual void didUploadVisibleHighResolutionTileOnImplTread() OVERRIDE { }
  virtual void setNeedsCommitOnImplThread() OVERRIDE { }
  virtual void setNeedsManageTilesOnImplThread() OVERRIDE { }
  virtual void postAnimationEventsToMainThreadOnImplThread(
      scoped_ptr<AnimationEventsVector>,
      base::Time) OVERRIDE { }
  virtual bool reduceContentsTextureMemoryOnImplThread(size_t, int) OVERRIDE;
  virtual void sendManagedMemoryStats() OVERRIDE { }
  virtual bool isInsideDraw() OVERRIDE;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_CLIENT_H_
