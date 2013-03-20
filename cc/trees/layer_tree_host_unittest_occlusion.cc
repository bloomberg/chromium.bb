// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "cc/layers/layer.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/occlusion_tracker_test_common.h"

namespace cc {
namespace {

class TestLayer : public Layer {
 public:
  static scoped_refptr<TestLayer> Create() {
    return make_scoped_refptr(new TestLayer());
  }

  virtual void Update(
      ResourceUpdateQueue* update_queue,
      const OcclusionTracker* occlusion,
      RenderingStats* stats) OVERRIDE {
    if (!occlusion)
      return;

    // Gain access to internals of the OcclusionTracker.
    const TestOcclusionTracker* test_occlusion =
        static_cast<const TestOcclusionTracker*>(occlusion);
    occlusion_ = UnionRegions(
        test_occlusion->occlusion_from_inside_target(),
        test_occlusion->occlusion_from_outside_target());
  }

  const Region& occlusion() const { return occlusion_; }
  const Region& expected_occlusion() const { return expected_occlusion_; }
  void set_expected_occlusion(const Region& occlusion) {
    expected_occlusion_ = occlusion;
  }

 private:
  TestLayer() : Layer() {
    SetIsDrawable(true);
  }
  virtual ~TestLayer() { }

  Region occlusion_;
  Region expected_occlusion_;
};

class LayerTreeHostOcclusionTest : public LayerTreeTest {
 public:

  LayerTreeHostOcclusionTest()
      : root_(TestLayer::Create()),
        child_(TestLayer::Create()),
        child2_(TestLayer::Create()),
        grand_child_(TestLayer::Create()),
        mask_(TestLayer::Create()) {
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    TestLayer* root = static_cast<TestLayer*>(layer_tree_host()->root_layer());
    VerifyOcclusion(root);

    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

  void VerifyOcclusion(TestLayer* layer) const {
    EXPECT_EQ(layer->expected_occlusion().ToString(),
              layer->occlusion().ToString());

    for (size_t i = 0; i < layer->children().size(); ++i) {
      TestLayer* child = static_cast<TestLayer*>(layer->children()[i].get());
      VerifyOcclusion(child);
    }
  }

  void SetLayerPropertiesForTesting(
      TestLayer* layer, TestLayer* parent, const gfx::Transform& transform,
      const gfx::PointF& position, const gfx::Size& bounds, bool opaque) const {
    layer->RemoveAllChildren();
    if (parent)
      parent->AddChild(layer);
    layer->SetTransform(transform);
    layer->SetPosition(position);
    layer->SetBounds(bounds);
    layer->SetContentsOpaque(opaque);

    layer->SetAnchorPoint(gfx::PointF());
  }

 protected:
  scoped_refptr<TestLayer> root_;
  scoped_refptr<TestLayer> child_;
  scoped_refptr<TestLayer> child2_;
  scoped_refptr<TestLayer> grand_child_;
  scoped_refptr<TestLayer> mask_;

  gfx::Transform identity_matrix_;
};


class LayerTreeHostOcclusionTestOcclusionSurfaceClipping :
    public LayerTreeHostOcclusionTest {
 public:
  virtual void SetupTree() OVERRIDE {
    // The child layer is a surface and the grandChild is opaque, but clipped to
    // the child and root
    SetLayerPropertiesForTesting(
        root_.get(), NULL, identity_matrix_,
        gfx::PointF(0.f, 0.f), gfx::Size(200, 200), true);
    SetLayerPropertiesForTesting(
        child_.get(), root_.get(), identity_matrix_,
        gfx::PointF(10.f, 10.f), gfx::Size(500, 500), false);
    SetLayerPropertiesForTesting(
        grand_child_.get(), child_.get(), identity_matrix_,
        gfx::PointF(-10.f, -10.f), gfx::Size(20, 500), true);

    child_->SetMasksToBounds(true);
    child_->SetForceRenderSurface(true);
    
    child_->set_expected_occlusion(gfx::Rect(0, 0, 10, 190));
    root_->set_expected_occlusion(gfx::Rect(10, 10, 10, 190));

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeTest::SetupTree();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostOcclusionTestOcclusionSurfaceClipping)

class LayerTreeHostOcclusionTestOcclusionSurfaceClippingOpaque :
    public LayerTreeHostOcclusionTest {
 public:
  virtual void SetupTree() OVERRIDE {
    // If the child layer is opaque, then it adds to the occlusion seen by the
    // root_.
    SetLayerPropertiesForTesting(
        root_.get(), NULL, identity_matrix_,
        gfx::PointF(0.f, 0.f), gfx::Size(200, 200), true);
    SetLayerPropertiesForTesting(
        child_.get(), root_.get(), identity_matrix_,
        gfx::PointF(10.f, 10.f), gfx::Size(500, 500), true);
    SetLayerPropertiesForTesting(
        grand_child_.get(), child_.get(), identity_matrix_,
        gfx::PointF(-10.f, -10.f), gfx::Size(20, 500), true);

    child_->SetMasksToBounds(true);
    child_->SetForceRenderSurface(true);

    child_->set_expected_occlusion(gfx::Rect(0, 0, 10, 190));
    root_->set_expected_occlusion(gfx::Rect(10, 10, 190, 190));

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeTest::SetupTree();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostOcclusionTestOcclusionSurfaceClippingOpaque);

class LayerTreeHostOcclusionTestOcclusionTwoChildren :
    public LayerTreeHostOcclusionTest {
 public:
  virtual void SetupTree() OVERRIDE {
    // Add a second child to the root layer and the regions should merge
    SetLayerPropertiesForTesting(
        root_.get(), NULL, identity_matrix_,
        gfx::PointF(0.f, 0.f), gfx::Size(200, 200), true);
    SetLayerPropertiesForTesting(
        child_.get(), root_.get(), identity_matrix_,
        gfx::PointF(10.f, 10.f), gfx::Size(500, 500), false);
    SetLayerPropertiesForTesting(
        grand_child_.get(), child_.get(), identity_matrix_,
        gfx::PointF(-10.f, -10.f), gfx::Size(20, 500), true);
    SetLayerPropertiesForTesting(
        child2_.get(), root_.get(), identity_matrix_,
        gfx::PointF(20.f, 10.f), gfx::Size(10, 500), true);

    child_->SetMasksToBounds(true);
    child_->SetForceRenderSurface(true);

    grand_child_->set_expected_occlusion(gfx::Rect(10, 0, 10, 190));
    child_->set_expected_occlusion(gfx::Rect(0, 0, 20, 190));
    root_->set_expected_occlusion(gfx::Rect(10, 10, 20, 190));

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeTest::SetupTree();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostOcclusionTestOcclusionTwoChildren)

class LayerTreeHostOcclusionTestOcclusionMask :
    public LayerTreeHostOcclusionTest {
 public:
  virtual void SetupTree() OVERRIDE {
    // If the child layer has a mask on it, then it shouldn't contribute to
    // occlusion on stuff below it.
    SetLayerPropertiesForTesting(
        root_.get(), NULL, identity_matrix_,
        gfx::PointF(0.f, 0.f), gfx::Size(200, 200), true);
    SetLayerPropertiesForTesting(
        child2_.get(), root_.get(), identity_matrix_,
        gfx::PointF(10.f, 10.f), gfx::Size(500, 500), true);
    SetLayerPropertiesForTesting(
        child_.get(), root_.get(), identity_matrix_,
        gfx::PointF(20.f, 20.f), gfx::Size(500, 500), true);
    SetLayerPropertiesForTesting(
        grand_child_.get(), child_.get(), identity_matrix_,
        gfx::PointF(-10.f, -10.f), gfx::Size(500, 500), true);

    child_->SetMasksToBounds(true);
    child_->SetForceRenderSurface(true);
    child_->SetMaskLayer(mask_.get());

    child_->set_expected_occlusion(gfx::Rect(0, 0, 180, 180));
    root_->set_expected_occlusion(gfx::Rect(10, 10, 190, 190));

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeTest::SetupTree();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostOcclusionTestOcclusionMask)

class LayerTreeHostOcclusionTestOcclusionMaskBelowOcclusion :
    public LayerTreeHostOcclusionTest {
 public:
  virtual void SetupTree() OVERRIDE {
    // If the child layer with a mask is below child2, then child2 should
    // contribute to occlusion on everything, and child shouldn't contribute
    // to the root_.
    SetLayerPropertiesForTesting(
        root_.get(), NULL, identity_matrix_,
        gfx::PointF(0.f, 0.f), gfx::Size(200, 200), true);
    SetLayerPropertiesForTesting(
        child_.get(), root_.get(), identity_matrix_,
        gfx::PointF(10.f, 10.f), gfx::Size(500, 500), true);
    SetLayerPropertiesForTesting(
        grand_child_.get(), child_.get(), identity_matrix_,
        gfx::PointF(-10.f, -10.f), gfx::Size(20, 500), true);
    SetLayerPropertiesForTesting(
        child2_.get(), root_.get(), identity_matrix_,
        gfx::PointF(20.f, 10.f), gfx::Size(10, 500), true);

    child_->SetMasksToBounds(true);
    child_->SetForceRenderSurface(true);
    child_->SetMaskLayer(mask_.get());
  
    grand_child_->set_expected_occlusion(gfx::Rect(10, 0, 10, 190));
    child_->set_expected_occlusion(gfx::Rect(0, 0, 20, 190));
    root_->set_expected_occlusion(gfx::Rect(20, 10, 10, 190));

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeTest::SetupTree();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostOcclusionTestOcclusionMaskBelowOcclusion)

class LayerTreeHostOcclusionTestOcclusionOpacity :
    public LayerTreeHostOcclusionTest {
 public:
  virtual void SetupTree() OVERRIDE {
    // If the child layer has a non-opaque opacity, then it shouldn't
    // contribute to occlusion on stuff below it
    SetLayerPropertiesForTesting(
        root_.get(), NULL, identity_matrix_,
        gfx::PointF(0.f, 0.f), gfx::Size(200, 200), true);
    SetLayerPropertiesForTesting(
        child2_.get(), root_.get(), identity_matrix_,
        gfx::PointF(20.f, 10.f), gfx::Size(10, 500), true);
    SetLayerPropertiesForTesting(
        child_.get(), root_.get(), identity_matrix_,
        gfx::PointF(10.f, 10.f), gfx::Size(500, 500), true);
    SetLayerPropertiesForTesting(
        grand_child_.get(), child_.get(), identity_matrix_,
        gfx::PointF(-10.f, -10.f), gfx::Size(20, 500), true);

    child_->SetMasksToBounds(true);
    child_->SetForceRenderSurface(true);
    child_->SetOpacity(0.5f);

    child_->set_expected_occlusion(gfx::Rect(0, 0, 10, 190));
    root_->set_expected_occlusion(gfx::Rect(20, 10, 10, 190));

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeTest::SetupTree();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostOcclusionTestOcclusionOpacity)

class LayerTreeHostOcclusionTestOcclusionOpacityBelowOcclusion :
    public LayerTreeHostOcclusionTest {
 public:
  virtual void SetupTree() OVERRIDE {
    // If the child layer with non-opaque opacity is below child2, then
    // child2 should contribute to occlusion on everything, and child shouldn't
    // contribute to the root_.
    SetLayerPropertiesForTesting(
        root_.get(), NULL, identity_matrix_,
        gfx::PointF(0.f, 0.f), gfx::Size(200, 200), true);
    SetLayerPropertiesForTesting(
        child_.get(), root_.get(), identity_matrix_,
        gfx::PointF(10.f, 10.f), gfx::Size(500, 500), true);
    SetLayerPropertiesForTesting(
        grand_child_.get(), child_.get(), identity_matrix_,
        gfx::PointF(-10.f, -10.f), gfx::Size(20, 500), true);
    SetLayerPropertiesForTesting(
        child2_.get(), root_.get(), identity_matrix_,
        gfx::PointF(20.f, 10.f), gfx::Size(10, 500), true);

    child_->SetMasksToBounds(true);
    child_->SetForceRenderSurface(true);
    child_->SetOpacity(0.5f);

    grand_child_->set_expected_occlusion(gfx::Rect(10, 0, 10, 190));
    child_->set_expected_occlusion(gfx::Rect(0, 0, 20, 190));
    root_->set_expected_occlusion(gfx::Rect(20, 10, 10, 190));

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeTest::SetupTree();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostOcclusionTestOcclusionOpacityBelowOcclusion)

class LayerTreeHostOcclusionTestOcclusionOpacityFilter :
    public LayerTreeHostOcclusionTest {
 public:
  virtual void SetupTree() OVERRIDE {
    gfx::Transform childTransform;
    childTransform.Translate(250.0, 250.0);
    childTransform.Rotate(90.0);
    childTransform.Translate(-250.0, -250.0);

    WebKit::WebFilterOperations filters;
    filters.append(WebKit::WebFilterOperation::createOpacityFilter(0.5));

    // If the child layer has a filter that changes alpha values, and is below
    // child2, then child2 should contribute to occlusion on everything,
    // and child shouldn't contribute to the root
    SetLayerPropertiesForTesting(
        root_.get(), NULL, identity_matrix_,
        gfx::PointF(0.f, 0.f), gfx::Size(200, 200), true);
    SetLayerPropertiesForTesting(
        child_.get(), root_.get(), childTransform,
        gfx::PointF(30.f, 30.f), gfx::Size(500, 500), true);
    SetLayerPropertiesForTesting(
        grand_child_.get(), child_.get(), identity_matrix_,
        gfx::PointF(10.f, 10.f), gfx::Size(500, 500), true);
    SetLayerPropertiesForTesting(
        child2_.get(), root_.get(), identity_matrix_,
        gfx::PointF(10.f, 70.f), gfx::Size(500, 500), true);

    child_->SetMasksToBounds(true);
    child_->SetFilters(filters);

    grand_child_->set_expected_occlusion(gfx::Rect(40, 330, 130, 190));
    child_->set_expected_occlusion(UnionRegions(
        gfx::Rect(10, 330, 160, 170), gfx::Rect(40, 500, 130, 20)));
    root_->set_expected_occlusion(gfx::Rect(10, 70, 190, 130));

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeTest::SetupTree();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostOcclusionTestOcclusionOpacityFilter)

class LayerTreeHostOcclusionTestOcclusionBlurFilter :
    public LayerTreeHostOcclusionTest {
 public:
  virtual void SetupTree() OVERRIDE {
    gfx::Transform childTransform;
    childTransform.Translate(250.0, 250.0);
    childTransform.Rotate(90.0);
    childTransform.Translate(-250.0, -250.0);

    WebKit::WebFilterOperations filters;
    filters.append(WebKit::WebFilterOperation::createBlurFilter(10));

    // If the child layer has a filter that moves pixels/changes alpha, and is
    // below child2, then child should not inherit occlusion from outside its
    // subtree, and should not contribute to the root
    SetLayerPropertiesForTesting(
        root_.get(), NULL, identity_matrix_,
        gfx::PointF(0.f, 0.f), gfx::Size(200, 200), true);
    SetLayerPropertiesForTesting(
        child_.get(), root_.get(), childTransform,
        gfx::PointF(30.f, 30.f), gfx::Size(500, 500), true);
    SetLayerPropertiesForTesting(
        grand_child_.get(), child_.get(), identity_matrix_,
        gfx::PointF(10.f, 10.f), gfx::Size(500, 500), true);
    SetLayerPropertiesForTesting(
        child2_.get(), root_.get(), identity_matrix_,
        gfx::PointF(10.f, 70.f), gfx::Size(500, 500), true);

    child_->SetMasksToBounds(true);
    child_->SetFilters(filters);

    child_->set_expected_occlusion(gfx::Rect(10, 330, 160, 170));
    root_->set_expected_occlusion(gfx::Rect(10, 70, 190, 130));

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeTest::SetupTree();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(
    LayerTreeHostOcclusionTestOcclusionBlurFilter)

class LayerTreeHostOcclusionTestManySurfaces :
    public LayerTreeHostOcclusionTest {
 public:
  virtual void SetupTree() OVERRIDE {
    // We create enough RenderSurfaces that it will trigger Vector reallocation
    // while computing occlusion.
    std::vector<scoped_refptr<TestLayer> > layers;
    int num_surfaces = 200;
    int root_width = 400;
    int root_height = 400;

    for (int i = 0; i < num_surfaces; ++i) {
      layers.push_back(TestLayer::Create());
      if (!i) {
        SetLayerPropertiesForTesting(
            layers.back().get(), NULL, identity_matrix_,
            gfx::PointF(0.f, 0.f),
            gfx::Size(root_width, root_height), true);
        layers.back()->CreateRenderSurface();
      } else {
        SetLayerPropertiesForTesting(
            layers.back().get(), layers[layers.size() - 2].get(),
            identity_matrix_,
            gfx::PointF(1.f, 1.f),
            gfx::Size(root_width-i, root_height-i), true);
        layers.back()->SetForceRenderSurface(true);
      }
    }

    for (int i = 1; i < num_surfaces; ++i) {
      scoped_refptr<TestLayer> child = TestLayer::Create();
      SetLayerPropertiesForTesting(
          child.get(), layers[i].get(), identity_matrix_,
          gfx::PointF(0.f, 0.f), gfx::Size(root_width, root_height), false);
    }

    for (int i = 0; i < num_surfaces-1; ++i) {
      gfx::Rect expected_occlusion(1, 1, root_width-i-1, root_height-i-1);
      layers[i]->set_expected_occlusion(expected_occlusion);
    }

    layer_tree_host()->SetRootLayer(layers[0]);
    LayerTreeTest::SetupTree();
  }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostOcclusionTestManySurfaces)

}  // namespace
}  // namespace cc
