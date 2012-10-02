// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "FakeWebGraphicsContext3D.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ContextThatCountsMakeCurrents : public WebKit::FakeWebGraphicsContext3D {
public:
    ContextThatCountsMakeCurrents() : m_makeCurrentCount(0) { }
    virtual bool makeContextCurrent() OVERRIDE
    {
        m_makeCurrentCount++;
        return true;
    }
    int makeCurrentCount() const { return m_makeCurrentCount; }

private:
    int m_makeCurrentCount;
};


TEST(FakeGraphicsContext3DTest, ContextCreationShouldNotMakeCurrent)
{
    scoped_ptr<ContextThatCountsMakeCurrents> context(new ContextThatCountsMakeCurrents);
    EXPECT_TRUE(context.get());
    EXPECT_EQ(0, context->makeCurrentCount());
}
