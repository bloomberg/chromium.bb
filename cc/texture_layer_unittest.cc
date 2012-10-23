// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/texture_layer.h"

#include "cc/layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/web_compositor_initializer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace cc;
using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

namespace {

class MockLayerImplTreeHost : public LayerTreeHost {
public:
    MockLayerImplTreeHost()
        : LayerTreeHost(&m_fakeClient, LayerTreeSettings())
    {
        initialize();
    }

    MOCK_METHOD0(acquireLayerTextures, void());

private:
    FakeLayerImplTreeHostClient m_fakeClient;
};


class TextureLayerTest : public testing::Test {
public:
    TextureLayerTest()
        : m_compositorInitializer(0)
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

        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.reset();
    }

    scoped_ptr<MockLayerImplTreeHost> m_layerTreeHost;
private:
    WebKitTests::WebCompositorInitializer m_compositorInitializer;
};

TEST_F(TextureLayerTest, syncImplWhenChangingTextureId)
{
    scoped_refptr<TextureLayer> testLayer = TextureLayer::create(0);
    ASSERT_TRUE(testLayer);

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AnyNumber());
    m_layerTreeHost->setRootLayer(testLayer);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
    EXPECT_EQ(testLayer->layerTreeHost(), m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    testLayer->setTextureId(1);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AtLeast(1));
    testLayer->setTextureId(2);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AtLeast(1));
    testLayer->setTextureId(0);
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
    m_layerTreeHost->setRootLayer(rootLayer);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    testLayer->removeFromParent();
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    childLayer->addChild(testLayer);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    testLayer->setTextureId(1);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AtLeast(1));
    testLayer->removeFromParent();
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
}

} // anonymous namespace
