// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/thread_task.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class Mock {
public:
    MOCK_METHOD0(method0, void());
    MOCK_METHOD1(method1, void(int a1));
    MOCK_METHOD2(method2, void(int a1, int a2));
    MOCK_METHOD3(method3, void(int a1, int a2, int a3));
    MOCK_METHOD4(method4, void(int a1, int a2, int a3, int a4));
    MOCK_METHOD5(method5, void(int a1, int a2, int a3, int a4, int a5));
};

TEST(ThreadTaskTest, runnableMethods)
{
    Mock mock;
    EXPECT_CALL(mock, method0()).Times(1);
    EXPECT_CALL(mock, method1(9)).Times(1);
    EXPECT_CALL(mock, method2(9, 8)).Times(1);
    EXPECT_CALL(mock, method3(9, 8, 7)).Times(1);
    EXPECT_CALL(mock, method4(9, 8, 7, 6)).Times(1);
    EXPECT_CALL(mock, method5(9, 8, 7, 6, 5)).Times(1);

    createThreadTask(&mock, &Mock::method0)->performTask();
    createThreadTask(&mock, &Mock::method1, 9)->performTask();
    createThreadTask(&mock, &Mock::method2, 9, 8)->performTask();
    createThreadTask(&mock, &Mock::method3, 9, 8, 7)->performTask();
    createThreadTask(&mock, &Mock::method4, 9, 8, 7, 6)->performTask();
    createThreadTask(&mock, &Mock::method5, 9, 8, 7, 6, 5)->performTask();
}

}  // namespace cc
