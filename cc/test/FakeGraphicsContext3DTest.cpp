// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "FakeWebGraphicsContext3D.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

using namespace cc;
using namespace WebCore;
using namespace WebKit;

class ContextThatCountsMakeCurrents : public FakeWebGraphicsContext3D {
public:
    ContextThatCountsMakeCurrents() : m_makeCurrentCount(0) { }
    virtual bool makeContextCurrent()
    {
        m_makeCurrentCount++;
        return true;
    }
    int makeCurrentCount() { return m_makeCurrentCount; }
private:
    int m_makeCurrentCount;
};


TEST(FakeGraphicsContext3DTest, ContextCreationShouldNotMakeCurrent)
{
    OwnPtr<ContextThatCountsMakeCurrents> context(adoptPtr(new ContextThatCountsMakeCurrents));
    EXPECT_TRUE(context);
    EXPECT_EQ(0, context->makeCurrentCount());
}
