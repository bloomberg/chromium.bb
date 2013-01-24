// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scrollbar_animation_controller_linear_fade.h"

#include "cc/scrollbar_layer_impl.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_web_scrollbar_theme_geometry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class ScrollbarAnimationControllerLinearFadeTest : public testing::Test {
public:
    ScrollbarAnimationControllerLinearFadeTest()
        : m_hostImpl(&m_proxy)
    {
    }

protected:
    virtual void SetUp()
    {
        m_scrollLayer = LayerImpl::create(m_hostImpl.activeTree(), 1);
        scoped_ptr<ScrollbarGeometryFixedThumb> geometry(ScrollbarGeometryFixedThumb::create(FakeWebScrollbarThemeGeometry::create(false)));
        m_scrollbarLayer = ScrollbarLayerImpl::create(m_hostImpl.activeTree(), 2, geometry.Pass());

        m_scrollLayer->setMaxScrollOffset(gfx::Vector2d(50, 50));
        m_scrollLayer->setBounds(gfx::Size(50, 50));
        m_scrollLayer->setHorizontalScrollbarLayer(m_scrollbarLayer.get());

        m_scrollbarController = ScrollbarAnimationControllerLinearFade::create(m_scrollLayer.get(), 2, 3);
    }

    FakeImplProxy m_proxy;
    FakeLayerTreeHostImpl m_hostImpl;
    scoped_ptr<ScrollbarAnimationControllerLinearFade> m_scrollbarController;
    scoped_ptr<LayerImpl> m_scrollLayer;
    scoped_ptr<ScrollbarLayerImpl> m_scrollbarLayer;

};

TEST_F(ScrollbarAnimationControllerLinearFadeTest, verifyHiddenInBegin)
{
    m_scrollbarController->animate(base::TimeTicks());
    EXPECT_FLOAT_EQ(0, m_scrollbarLayer->opacity());
}

TEST_F(ScrollbarAnimationControllerLinearFadeTest, verifyAwakenByScroll)
{
    base::TimeTicks time;
    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->didUpdateScrollOffset(time);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->didUpdateScrollOffset(time);

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(2.0f / 3.0f, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1.0f / 3.0f, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->didUpdateScrollOffset(time);
    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(2.0f / 3.0f, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1.0f / 3.0f, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(0, m_scrollbarLayer->opacity());
}

TEST_F(ScrollbarAnimationControllerLinearFadeTest, verifyForceAwakenByPinch)
{
    base::TimeTicks time;
    m_scrollbarController->didPinchGestureUpdate(time);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    m_scrollbarController->didUpdateScrollOffset(time);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->didPinchGestureEnd(time);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(2);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(2 / 3.0f, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1 / 3.0f, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(0, m_scrollbarLayer->opacity());

}

}  // namespace
}  // namespace cc
