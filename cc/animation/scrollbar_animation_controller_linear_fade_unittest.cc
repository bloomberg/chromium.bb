// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scrollbar_animation_controller_linear_fade.h"

#include "cc/scrollbar_layer_impl.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_web_scrollbar_theme_geometry.h"
#include "cc/trees/single_thread_proxy.h"
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
        m_scrollLayer = LayerImpl::Create(m_hostImpl.active_tree(), 1);
        scoped_ptr<ScrollbarGeometryFixedThumb> geometry(ScrollbarGeometryFixedThumb::create(FakeWebScrollbarThemeGeometry::create(false)));
        m_scrollbarLayer = ScrollbarLayerImpl::Create(m_hostImpl.active_tree(), 2, geometry.Pass());

        m_scrollLayer->SetMaxScrollOffset(gfx::Vector2d(50, 50));
        m_scrollLayer->SetBounds(gfx::Size(50, 50));
        m_scrollLayer->SetHorizontalScrollbarLayer(m_scrollbarLayer.get());

        m_scrollbarController = ScrollbarAnimationControllerLinearFade::create(m_scrollLayer.get(), base::TimeDelta::FromSeconds(2), base::TimeDelta::FromSeconds(3));
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

TEST_F(ScrollbarAnimationControllerLinearFadeTest, verifyAwakenByScrollGesture)
{
    base::TimeTicks time;
    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->didScrollGestureBegin();
    EXPECT_TRUE(m_scrollbarController->isScrollGestureInProgress());
    EXPECT_FALSE(m_scrollbarController->isAnimating());
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(100);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->didScrollGestureEnd(time);

    EXPECT_FALSE(m_scrollbarController->isScrollGestureInProgress());
    EXPECT_TRUE(m_scrollbarController->isAnimating());
    EXPECT_EQ(2, m_scrollbarController->delayBeforeStart(time).InSeconds());

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

    m_scrollbarController->didScrollGestureBegin();
    m_scrollbarController->didScrollGestureEnd(time);

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

TEST_F(ScrollbarAnimationControllerLinearFadeTest, verifyAwakenByProgrammaticScroll)
{
    base::TimeTicks time;
    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->didProgrammaticallyUpdateScroll(time);
    EXPECT_FALSE(m_scrollbarController->isScrollGestureInProgress());
    EXPECT_TRUE(m_scrollbarController->isAnimating());
    EXPECT_EQ(2, m_scrollbarController->delayBeforeStart(time).InSeconds());
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());

    time += base::TimeDelta::FromSeconds(1);
    m_scrollbarController->animate(time);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->didProgrammaticallyUpdateScroll(time);

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
    m_scrollbarController->didProgrammaticallyUpdateScroll(time);
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

}  // namespace
}  // namespace cc
