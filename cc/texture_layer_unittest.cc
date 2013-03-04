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
        gpu::Mailbox m1;
        m1.SetName(reinterpret_cast<const int8*>(m_mailboxName1.data()));
        m_mailbox1 = TextureMailbox(m1, m_releaseMailbox1, m_syncPoint1);
        gpu::Mailbox m2;
        m2.SetName(reinterpret_cast<const int8*>(m_mailboxName2.data()));
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
        : m_callbackCount(0)
        , m_commitCount(0)
    {
    }

    // Make sure callback is received on main and doesn't block the impl thread.
    void releaseCallback(unsigned syncPoint) {
        EXPECT_EQ(true, proxy()->isMainThread());
        ++m_callbackCount;
    }

    void setMailbox(char mailbox_char) {
        TextureMailbox mailbox(
            std::string(64, mailbox_char),
            base::Bind(
                &TextureLayerImplWithMailboxThreadedCallback::releaseCallback,
                base::Unretained(this)));
        m_layer->setTextureMailbox(mailbox);
    }

    virtual void beginTest() OVERRIDE
    {
        gfx::Size bounds(100, 100);
        m_root = Layer::create();
        m_root->setAnchorPoint(gfx::PointF());
        m_root->setBounds(bounds);

        m_layer = TextureLayer::createForMailbox();
        m_layer->setIsDrawable(true);
        m_layer->setAnchorPoint(gfx::PointF());
        m_layer->setBounds(bounds);

        m_root->addChild(m_layer);
        m_layerTreeHost->setRootLayer(m_root);
        m_layerTreeHost->setViewportSize(bounds, bounds);
        setMailbox('1');
        EXPECT_EQ(0, m_callbackCount);

        // Case #1: change mailbox before the commit. The old mailbox should be
        // released immediately.
        setMailbox('2');
        EXPECT_EQ(1, m_callbackCount);
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommit() OVERRIDE
    {
        ++m_commitCount;
        switch (m_commitCount) {
        case 1:
            // Case #2: change mailbox after the commit (and draw), where the
            // layer draws. The old mailbox should be released during the next
            // commit.
            setMailbox('3');
            EXPECT_EQ(1, m_callbackCount);
            break;
        case 2:
            // Old mailbox was released, task was posted, but won't execute
            // until this didCommit returns.
            // TODO(piman): fix this.
            EXPECT_EQ(1, m_callbackCount);
            m_layerTreeHost->setNeedsCommit();
            break;
        case 3:
            EXPECT_EQ(2, m_callbackCount);
            // Case #3: change mailbox when the layer doesn't draw. The old
            // mailbox should be released during the next commit.
            m_layer->setBounds(gfx::Size());
            setMailbox('4');
            break;
        case 4:
            // Old mailbox was released, task was posted, but won't execute
            // until this didCommit returns.
            // TODO(piman): fix this.
            EXPECT_EQ(2, m_callbackCount);
            m_layerTreeHost->setNeedsCommit();
            break;
        case 5:
            EXPECT_EQ(3, m_callbackCount);
            // Case #4: release mailbox that was committed but never drawn. The
            // old mailbox should be released during the next commit.
            m_layer->setTextureMailbox(TextureMailbox());
            break;
        case 6:
            // Old mailbox was released, task was posted, but won't execute
            // until this didCommit returns.
            // TODO(piman): fix this.
            EXPECT_EQ(3, m_callbackCount);
            m_layerTreeHost->setNeedsCommit();
            break;
        case 7:
            EXPECT_EQ(4, m_callbackCount);
            endTest();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_callbackCount;
    int m_commitCount;
    scoped_refptr<Layer> m_root;
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
    m_hostImpl.createPendingTree();
    scoped_ptr<TextureLayerImpl> pendingLayer;
    pendingLayer = TextureLayerImpl::create(m_hostImpl.pendingTree(), 1, true);
    ASSERT_TRUE(pendingLayer);

    scoped_ptr<LayerImpl> activeLayer(pendingLayer->createLayerImpl(
        m_hostImpl.activeTree()));
    ASSERT_TRUE(activeLayer);

    pendingLayer->setTextureMailbox(m_testData.m_mailbox1);

    // Test multiple commits without an activation.
    EXPECT_CALL(m_testData.m_mockCallback,
                Release(m_testData.m_mailboxName1,
                        m_testData.m_syncPoint1)).Times(1);
    pendingLayer->setTextureMailbox(m_testData.m_mailbox2);
    Mock::VerifyAndClearExpectations(&m_testData.m_mockCallback);

    // Test callback after activation.
    pendingLayer->pushPropertiesTo(activeLayer.get());
    activeLayer->didBecomeActive();

    EXPECT_CALL(m_testData.m_mockCallback,
                Release(_, _)).Times(0);
    pendingLayer->setTextureMailbox(m_testData.m_mailbox1);
    Mock::VerifyAndClearExpectations(&m_testData.m_mockCallback);

    EXPECT_CALL(m_testData.m_mockCallback,
                Release(m_testData.m_mailboxName2, _)).Times(1);
    pendingLayer->pushPropertiesTo(activeLayer.get());
    activeLayer->didBecomeActive();
    Mock::VerifyAndClearExpectations(&m_testData.m_mockCallback);

    // Test resetting the mailbox.
    EXPECT_CALL(m_testData.m_mockCallback,
                Release(m_testData.m_mailboxName1, _)).Times(1);
    pendingLayer->setTextureMailbox(TextureMailbox());
    pendingLayer->pushPropertiesTo(activeLayer.get());
    activeLayer->didBecomeActive();
    Mock::VerifyAndClearExpectations(&m_testData.m_mockCallback);

    // Test destructor.
    EXPECT_CALL(m_testData.m_mockCallback,
                Release(m_testData.m_mailboxName1,
                        m_testData.m_syncPoint1)).Times(1);
    pendingLayer->setTextureMailbox(m_testData.m_mailbox1);
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
    TransferableResourceArray list;
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
