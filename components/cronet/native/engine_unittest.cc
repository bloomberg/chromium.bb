// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cronet_c.h"

#include "build/build_config.h"
#include "components/cronet/native/engine.h"
#include "components/cronet/native/generated/cronet.idl_impl_struct.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cronet {

namespace {

// App implementation of Cronet_Executor methods.
void TestExecutor_Execute(Cronet_ExecutorPtr self, Cronet_RunnablePtr command) {
  CHECK(self);
  Cronet_Runnable_Run(command);
  Cronet_Runnable_Destroy(command);
}

// App implementation of Cronet_RequestFinishedInfoListener methods.
void TestRequestInfoListener_OnRequestFinished(
    Cronet_RequestFinishedInfoListenerPtr self,
    Cronet_RequestFinishedInfoPtr request_info) {
  CHECK(self);
}

TEST(EngineUnitTest, HasNoRequestFinishedInfoListener) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());

  Cronet_Engine_Destroy(engine);
}

TEST(EngineUnitTest, HasRequestFinishedInfoListener) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  Cronet_RequestFinishedInfoListenerPtr listener =
      Cronet_RequestFinishedInfoListener_CreateWith(
          TestRequestInfoListener_OnRequestFinished);
  Cronet_ExecutorPtr executor =
      Cronet_Executor_CreateWith(TestExecutor_Execute);
  Cronet_Engine_AddRequestFinishedListener(engine, listener, executor);

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_TRUE(engine_impl->HasRequestFinishedListener());

  Cronet_Engine_RemoveRequestFinishedListener(engine, listener);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());
  Cronet_Executor_Destroy(executor);
  Cronet_RequestFinishedInfoListener_Destroy(listener);
  Cronet_Engine_Destroy(engine);
}

// EXPECT_DEBUG_DEATH(), used by the tests below, isn't available on iOS.
#if !defined(OS_IOS)

TEST(EngineUnitTest, AddNullRequestFinishedInfoListener) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  Cronet_ExecutorPtr executor =
      Cronet_Executor_CreateWith(TestExecutor_Execute);
  EXPECT_DEBUG_DEATH(
      Cronet_Engine_AddRequestFinishedListener(engine, nullptr, executor),
      "Both listener and executor must be non-null. listener: .* executor: "
      ".*\\.");

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());

  Cronet_Executor_Destroy(executor);
  Cronet_Engine_Destroy(engine);
}

TEST(EngineUnitTest, AddNullRequestFinishedInfoExecutor) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  Cronet_RequestFinishedInfoListenerPtr listener =
      Cronet_RequestFinishedInfoListener_CreateWith(
          TestRequestInfoListener_OnRequestFinished);
  EXPECT_DEBUG_DEATH(
      Cronet_Engine_AddRequestFinishedListener(engine, listener, nullptr),
      "Both listener and executor must be non-null. listener: .* executor: "
      ".*\\.");

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());

  Cronet_RequestFinishedInfoListener_Destroy(listener);
  Cronet_Engine_Destroy(engine);
}

TEST(EngineUnitTest, AddNullRequestFinishedInfoListenerAndExecutor) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  EXPECT_DEBUG_DEATH(
      Cronet_Engine_AddRequestFinishedListener(engine, nullptr, nullptr),
      "Both listener and executor must be non-null. listener: .* executor: "
      ".*\\.");

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());

  Cronet_Engine_Destroy(engine);
}

TEST(EngineUnitTest, AddListenerTwice) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  Cronet_RequestFinishedInfoListenerPtr listener =
      Cronet_RequestFinishedInfoListener_CreateWith(
          TestRequestInfoListener_OnRequestFinished);
  Cronet_ExecutorPtr executor =
      Cronet_Executor_CreateWith(TestExecutor_Execute);
  Cronet_Engine_AddRequestFinishedListener(engine, listener, executor);
  EXPECT_DEBUG_DEATH(
      Cronet_Engine_AddRequestFinishedListener(engine, listener, executor),
      "Listener .* already registered with executor .*, \\*NOT\\* changing to "
      "new executor .*\\.");

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_TRUE(engine_impl->HasRequestFinishedListener());

  Cronet_Engine_RemoveRequestFinishedListener(engine, listener);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());
  Cronet_Executor_Destroy(executor);
  Cronet_RequestFinishedInfoListener_Destroy(listener);
  Cronet_Engine_Destroy(engine);
}

TEST(EngineUnitTest, RemoveNonexistentListener) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  Cronet_RequestFinishedInfoListenerPtr listener =
      Cronet_RequestFinishedInfoListener_CreateWith(
          TestRequestInfoListener_OnRequestFinished);
  EXPECT_DEBUG_DEATH(
      Cronet_Engine_RemoveRequestFinishedListener(engine, listener),
      "Asked to erase non-existent RequestFinishedInfoListener .*\\.");

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());

  Cronet_RequestFinishedInfoListener_Destroy(listener);
  Cronet_Engine_Destroy(engine);
}

TEST(EngineUnitTest, RemoveNonexistentListenerWithAddedListener) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  Cronet_RequestFinishedInfoListenerPtr listener =
      Cronet_RequestFinishedInfoListener_CreateWith(
          TestRequestInfoListener_OnRequestFinished);
  Cronet_RequestFinishedInfoListenerPtr listener2 =
      Cronet_RequestFinishedInfoListener_CreateWith(
          TestRequestInfoListener_OnRequestFinished);
  Cronet_ExecutorPtr executor =
      Cronet_Executor_CreateWith(TestExecutor_Execute);
  Cronet_Engine_AddRequestFinishedListener(engine, listener, executor);

  EXPECT_DEBUG_DEATH(
      Cronet_Engine_RemoveRequestFinishedListener(engine, listener2),
      "Asked to erase non-existent RequestFinishedInfoListener .*\\.");

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_TRUE(engine_impl->HasRequestFinishedListener());

  Cronet_Engine_RemoveRequestFinishedListener(engine, listener);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());
  Cronet_RequestFinishedInfoListener_Destroy(listener);
  Cronet_RequestFinishedInfoListener_Destroy(listener2);
  Cronet_Executor_Destroy(executor);
  Cronet_Engine_Destroy(engine);
}

TEST(EngineUnitTest, RemoveNullListener) {
  Cronet_EnginePtr engine = Cronet_Engine_Create();

  EXPECT_DEBUG_DEATH(
      Cronet_Engine_RemoveRequestFinishedListener(engine, nullptr),
      "Asked to erase non-existent RequestFinishedInfoListener .*\\.");

  auto* engine_impl = static_cast<Cronet_EngineImpl*>(engine);
  EXPECT_FALSE(engine_impl->HasRequestFinishedListener());

  Cronet_Engine_Destroy(engine);
}

#endif  // !defined(OS_IOS)

}  // namespace

}  // namespace cronet
