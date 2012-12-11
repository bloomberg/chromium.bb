// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/texture_layer.h"

#include "cc/layer_tree_host.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/texture_layer_impl.h"
#include "cc/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

namespace cc {
namespace {

class MockLayerImplTreeHost : public LayerTreeHost {
public:
    MockLayerImplTreeHost()
        : LayerTreeHost(&m_fakeClient, LayerTreeSettings())
    {
        initialize(scoped_ptr<Thread>(NULL));
    }

    MOCK_METHOD0(acquireLayerTextures, void());
    MOCK_METHOD0(setNeedsCommit, void());

private:
    FakeLayerImplTreeHostClient m_fakeClient;
};


class TextureLayerTest : public testing::Test {
public:
    TextureLayerTest()
        : m_hostImpl(&m_proxy)
    {
    }

protected:
    virtual void SetUp()
    {
        m_layerTreeHost.reset(new MockLayerImplTreeHost);
    }

    virtual void TearDown()
    {
        Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
        EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AnyNumber());
        EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AnyNumber());

        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.reset();
    }

    scoped_ptr<MockLayerImplTreeHost> m_layerTreeHost;
    FakeImplProxy m_proxy;
    FakeLayerTreeHostImpl m_hostImpl;
};

TEST_F(TextureLayerTest, syncImplWhenChangingTextureId)
{
    scoped_refptr<TextureLayer> testLayer = TextureLayer::create(0);
    ASSERT_TRUE(testLayer);

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AnyNumber());
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AnyNumber());
    m_layerTreeHost->setRootLayer(testLayer);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
    EXPECT_EQ(testLayer->layerTreeHost(), m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
    testLayer->setTextureId(1);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AtLeast(1));
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
    testLayer->setTextureId(2);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AtLeast(1));
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
    testLayer->setTextureId(0);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
}

TEST_F(TextureLayerTest, syncImplWhenDrawing)
{
    gfx::RectF dirtyRect(0, 0, 1, 1);

    scoped_refptr<TextureLayer> testLayer = TextureLayer::create(0);
    ASSERT_TRUE(testLayer);
    scoped_ptr<TextureLayerImpl> implLayer;
    implLayer = TextureLayerImpl::create(m_hostImpl.activeTree(), 1);
    ASSERT_TRUE(implLayer);

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AnyNumber());
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AnyNumber());
    m_layerTreeHost->setRootLayer(testLayer);
    testLayer->setTextureId(1);
    testLayer->setIsDrawable(true);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
    EXPECT_EQ(testLayer->layerTreeHost(), m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(1);
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(0);
    testLayer->willModifyTexture();
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(1);
    testLayer->setNeedsDisplayRect(dirtyRect);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(1);
    testLayer->pushPropertiesTo(implLayer.get()); // fake commit
    testLayer->setIsDrawable(false);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    // Verify that non-drawable layers don't signal the compositor,
    // except for the first draw after last commit, which must acquire
    // the texture.
    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(1);
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(0);
    testLayer->willModifyTexture();
    testLayer->setNeedsDisplayRect(dirtyRect);
    testLayer->pushPropertiesTo(implLayer.get()); // fake commit
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    // Second draw with layer in non-drawable state: no texture
    // acquisition.
    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(0);
    testLayer->willModifyTexture();
    testLayer->setNeedsDisplayRect(dirtyRect);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
}

TEST_F(TextureLayerTest, syncImplWhenRemovingFromTree)
{
    scoped_refptr<Layer> rootLayer = Layer::create();
    ASSERT_TRUE(rootLayer);
    scoped_refptr<Layer> childLayer = Layer::create();
    ASSERT_TRUE(childLayer);
    rootLayer->addChild(childLayer);
    scoped_refptr<TextureLayer> testLayer = TextureLayer::create(0);
    ASSERT_TRUE(testLayer);
    testLayer->setTextureId(0);
    childLayer->addChild(testLayer);

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AnyNumber());
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AnyNumber());
    m_layerTreeHost->setRootLayer(rootLayer);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
    testLayer->removeFromParent();
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
    childLayer->addChild(testLayer);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
    testLayer->setTextureId(1);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AtLeast(1));
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
    testLayer->removeFromParent();
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
}

}  // namespace
}  // namespace cc
