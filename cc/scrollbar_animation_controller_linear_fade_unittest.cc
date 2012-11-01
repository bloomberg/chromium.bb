// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/scrollbar_animation_controller_linear_fade.h"

#include "cc/scrollbar_layer_impl.h"
#include "cc/single_thread_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace cc;

namespace {

class ScrollbarAnimationControllerLinearFadeTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        m_scrollLayer = LayerImpl::create(1);
        m_scrollLayer->addChild(LayerImpl::create(2));
        m_contentLayer = m_scrollLayer->children()[0];
        m_scrollbarLayer = ScrollbarLayerImpl::create(3);

        m_scrollLayer->setMaxScrollPosition(IntSize(50, 50));
        m_contentLayer->setBounds(IntSize(50, 50));

        m_scrollbarController = ScrollbarAnimationControllerLinearFade::create(m_scrollLayer.get(), 2, 3);
        m_scrollbarController->setHorizontalScrollbarLayer(m_scrollbarLayer.get());
    }

    DebugScopedSetImplThread implThread;

    scoped_ptr<ScrollbarAnimationControllerLinearFade> m_scrollbarController;
    scoped_ptr<LayerImpl> m_scrollLayer;
    LayerImpl* m_contentLayer;
    scoped_ptr<ScrollbarLayerImpl> m_scrollbarLayer;

};

TEST_F(ScrollbarAnimationControllerLinearFadeTest, verifyHiddenInBegin)
{
    m_scrollbarController->animate(0);
    EXPECT_FLOAT_EQ(0, m_scrollbarLayer->opacity());
    m_scrollbarController->updateScrollOffsetAtTime(m_scrollLayer.get(), 0);
    m_scrollbarController->animate(0);
    EXPECT_FLOAT_EQ(0, m_scrollbarLayer->opacity());
}

TEST_F(ScrollbarAnimationControllerLinearFadeTest, verifyAwakenByScroll)
{
    m_scrollLayer->setScrollDelta(IntSize(1, 1));
    m_scrollbarController->updateScrollOffsetAtTime(m_scrollLayer.get(), 0);
    m_scrollbarController->animate(0);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(1);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollLayer->setScrollDelta(IntSize(2, 2));
    m_scrollbarController->updateScrollOffsetAtTime(m_scrollLayer.get(), 1);
    m_scrollbarController->animate(2);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(3);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(4);
    // Note that we use 3.0f to avoid "argument is truncated from 'double' to
    // 'float'" warnings on Windows.
    EXPECT_FLOAT_EQ(2 / 3.0f, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(5);
    EXPECT_FLOAT_EQ(1 / 3.0f, m_scrollbarLayer->opacity());
    m_scrollLayer->setScrollDelta(IntSize(3, 3));
    m_scrollbarController->updateScrollOffsetAtTime(m_scrollLayer.get(), 5);
    m_scrollbarController->animate(6);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(7);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(8);
    EXPECT_FLOAT_EQ(2 / 3.0f, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(9);
    EXPECT_FLOAT_EQ(1 / 3.0f, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(10);
    EXPECT_FLOAT_EQ(0, m_scrollbarLayer->opacity());
}

TEST_F(ScrollbarAnimationControllerLinearFadeTest, verifyForceAwakenByPinch)
{
    m_scrollbarController->didPinchGestureBeginAtTime(0);
    m_scrollbarController->didPinchGestureUpdateAtTime(0);
    m_scrollbarController->animate(0);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(1);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollLayer->setScrollDelta(IntSize(1, 1));
    m_scrollbarController->updateScrollOffsetAtTime(m_scrollLayer.get(), 1);
    m_scrollbarController->animate(2);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(3);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(4);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(5);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(6);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->didPinchGestureEndAtTime(6);
    m_scrollbarController->animate(7);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(8);
    EXPECT_FLOAT_EQ(1, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(9);
    EXPECT_FLOAT_EQ(2 / 3.0f, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(10);
    EXPECT_FLOAT_EQ(1 / 3.0f, m_scrollbarLayer->opacity());
    m_scrollbarController->animate(11);
    EXPECT_FLOAT_EQ(0, m_scrollbarLayer->opacity());

}

}
