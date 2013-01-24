// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/texture_layer.h"

#include <string>

#include "base/callback.h"
#include "cc/layer_tree_host.h"
#include "cc/layer_tree_impl.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/layer_tree_test_common.h"
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
    implLayer = TextureLayerImpl::create(m_hostImpl.activeTree(), 1, false);
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

class MockMailboxCallback {
public:
    MOCK_METHOD2(Release, void(const std::string& mailbox, unsigned syncPoint));
};

struct CommonMailboxObjects {
    CommonMailboxObjects()
        : m_mailboxName1(64, '1')
        , m_mailboxName2(64, '2')
        , m_syncPoint1(1)
        , m_syncPoint2(2)
    {
        m_releaseMailbox1 = base::Bind(&MockMailboxCallback::Release,
                                       base::Unretained(&m_mockCallback),
                                       m_mailboxName1);
        m_releaseMailbox2 = base::Bind(&MockMailboxCallback::Release,
                                       base::Unretained(&m_mockCallback),
                                       m_mailboxName2);
        Mailbox m1;
        m1.setName(reinterpret_cast<const int8*>(m_mailboxName1.data()));
        m_mailbox1 = TextureMailbox(m1, m_releaseMailbox1, m_syncPoint1);
        Mailbox m2;
        m2.setName(reinterpret_cast<const int8*>(m_mailboxName2.data()));
        m_mailbox2 = TextureMailbox(m2, m_releaseMailbox2, m_syncPoint2);
    }

    std::string m_mailboxName1;
    std::string m_mailboxName2;
    MockMailboxCallback m_mockCallback;
    TextureMailbox::ReleaseCallback m_releaseMailbox1;
    TextureMailbox::ReleaseCallback m_releaseMailbox2;
    TextureMailbox m_mailbox1;
    TextureMailbox m_mailbox2;
    unsigned m_syncPoint1;
    unsigned m_syncPoint2;
};

class TextureLayerWithMailboxTest : public TextureLayerTest {
protected:
    virtual void TearDown()
    {
        Mock::VerifyAndClearExpectations(&m_testData.m_mockCallback);
        EXPECT_CALL(m_testData.m_mockCallback,
                    Release(m_testData.m_mailboxName1,
                            m_testData.m_syncPoint1)).Times(1);
        TextureLayerTest::TearDown();
    }

    CommonMailboxObjects m_testData;
};

TEST_F(TextureLayerWithMailboxTest, replaceMailboxOnMainThreadBeforeCommit)
{
    scoped_refptr<TextureLayer> testLayer = TextureLayer::createForMailbox();
    ASSERT_TRUE(testLayer);

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AnyNumber());
    m_layerTreeHost->setRootLayer(testLayer);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
    testLayer->setTextureMailbox(m_testData.m_mailbox1);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
    EXPECT_CALL(m_testData.m_mockCallback,
                Release(m_testData.m_mailboxName1,
                        m_testData.m_syncPoint1)).Times(1);
    testLayer->setTextureMailbox(m_testData.m_mailbox2);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
    Mock::VerifyAndClearExpectations(&m_testData.m_mockCallback);

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
    EXPECT_CALL(m_testData.m_mockCallback,
                Release(m_testData.m_mailboxName2,
                        m_testData.m_syncPoint2)).Times(1);
    testLayer->setTextureMailbox(TextureMailbox());
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
    Mock::VerifyAndClearExpectations(&m_testData.m_mockCallback);

    // Test destructor.
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
    testLayer->setTextureMailbox(m_testData.m_mailbox1);
}

class TextureLayerImplWithMailboxThreadedCallback : public ThreadedTest {
public:
    TextureLayerImplWithMailboxThreadedCallback()
        : m_resetMailbox(false)
    {
    }

    // Make sure callback is received on main and doesn't block the impl thread.
    void releaseCallback(unsigned syncPoint) {
        EXPECT_EQ(true, proxy()->isMainThread());
        endTest();
    }

    virtual void beginTest() OVERRIDE
    {
        m_layer = TextureLayer::createForMailbox();
        m_layer->setIsDrawable(true);
        m_layerTreeHost->setRootLayer(m_layer);
        TextureMailbox mailbox(
            std::string(64, '1'),
            base::Bind(
                &TextureLayerImplWithMailboxThreadedCallback::releaseCallback,
                base::Unretained(this)));
        m_layer->setTextureMailbox(mailbox);
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommit() OVERRIDE
    {
        if (m_resetMailbox)
            return;

        m_layer->setTextureMailbox(TextureMailbox());
        m_resetMailbox = true;
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    bool m_resetMailbox;
    scoped_refptr<TextureLayer> m_layer;
};

SINGLE_AND_MULTI_THREAD_TEST_F(TextureLayerImplWithMailboxThreadedCallback);

class TextureLayerImplWithMailboxTest : public TextureLayerTest {
protected:
    virtual void SetUp()
    {
        TextureLayerTest::SetUp();
        m_layerTreeHost.reset(new MockLayerImplTreeHost);
        EXPECT_TRUE(m_hostImpl.initializeRenderer(createFakeOutputSurface()));
    }

    CommonMailboxObjects m_testData;
};

TEST_F(TextureLayerImplWithMailboxTest, testImplLayerCallbacks)
{
    scoped_ptr<TextureLayerImpl> implLayer;
    implLayer = TextureLayerImpl::create(m_hostImpl.activeTree(), 1, true);
    ASSERT_TRUE(implLayer);

    // Test setting identical mailbox.
    EXPECT_CALL(m_testData.m_mockCallback, Release(_, _)).Times(0);
    implLayer->setTextureMailbox(m_testData.m_mailbox1);
    implLayer->setTextureMailbox(m_testData.m_mailbox1);
    Mock::VerifyAndClearExpectations(&m_testData.m_mockCallback);

    // Test multiple commits without a draw.
    EXPECT_CALL(m_testData.m_mockCallback,
                Release(m_testData.m_mailboxName1,
                        m_testData.m_syncPoint1)).Times(1);
    implLayer->setTextureMailbox(m_testData.m_mailbox2);
    Mock::VerifyAndClearExpectations(&m_testData.m_mockCallback);

    // Test resetting the mailbox.
    EXPECT_CALL(m_testData.m_mockCallback,
                Release(m_testData.m_mailboxName2,
                        m_testData.m_syncPoint2)).Times(1);
    implLayer->setTextureMailbox(TextureMailbox());
    Mock::VerifyAndClearExpectations(&m_testData.m_mockCallback);

    // Test destructor.
    EXPECT_CALL(m_testData.m_mockCallback,
                Release(m_testData.m_mailboxName1,
                        m_testData.m_syncPoint1)).Times(1);
    implLayer->setTextureMailbox(m_testData.m_mailbox1);
}

TEST_F(TextureLayerImplWithMailboxTest, testDestructorCallbackOnCreatedResource)
{
    scoped_ptr<TextureLayerImpl> implLayer;
    implLayer = TextureLayerImpl::create(m_hostImpl.activeTree(), 1, true);
    ASSERT_TRUE(implLayer);

    EXPECT_CALL(m_testData.m_mockCallback,
                Release(m_testData.m_mailboxName1, _)).Times(1);
    implLayer->setTextureMailbox(m_testData.m_mailbox1);
    implLayer->willDraw(m_hostImpl.activeTree()->resource_provider());
    implLayer->didDraw(m_hostImpl.activeTree()->resource_provider());
    implLayer->setTextureMailbox(TextureMailbox());
}

TEST_F(TextureLayerImplWithMailboxTest, testCallbackOnInUseResource)
{
    ResourceProvider *provider = m_hostImpl.activeTree()->resource_provider();
    ResourceProvider::ResourceId id =
        provider->createResourceFromTextureMailbox(m_testData.m_mailbox1);
    provider->allocateForTesting(id);

    // Transfer some resources to the parent.
    ResourceProvider::ResourceIdArray resourceIdsToTransfer;
    resourceIdsToTransfer.push_back(id);
    TransferableResourceList list;
    provider->prepareSendToParent(resourceIdsToTransfer, &list);
    EXPECT_TRUE(provider->inUseByConsumer(id));
    EXPECT_CALL(m_testData.m_mockCallback, Release(_, _)).Times(0);
    provider->deleteResource(id);
    Mock::VerifyAndClearExpectations(&m_testData.m_mockCallback);
    EXPECT_CALL(m_testData.m_mockCallback,
                Release(m_testData.m_mailboxName1, _)).Times(1);
    provider->receiveFromParent(list);
}

}  // namespace
}  // namespace cc
