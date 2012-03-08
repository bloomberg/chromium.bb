// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/common/gl_mock.h"
#include "gpu/command_buffer/common/gles2_cmd_format.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::SetArgumentPointee;

namespace gpu {
namespace gles2 {

class MockDecoder : public CommonDecoder {
 public:
  virtual ~MockDecoder() { }
  MOCK_METHOD3(DoCommand, error::Error(
     unsigned int command,
     unsigned int arg_count,
     const void* cmd_data));
  MOCK_CONST_METHOD1(GetCommandName, const char* (unsigned int command_id));
};

class QueryManagerTest : public testing::Test {
 public:
  static const int32 kSharedMemoryId = 401;
  static const size_t kSharedBufferSize = 2048;
  static const uint32 kSharedMemoryOffset = 132;
  static const int32 kInvalidSharedMemoryId = 402;
  static const uint32 kInvalidSharedMemoryOffset = kSharedBufferSize + 1;
  static const uint32 kInitialResult = 0xBDBDBDBDu;
  static const uint8 kInitialMemoryValue = 0xBDu;

  QueryManagerTest() {
  }
  ~QueryManagerTest() {
  }

 protected:
  virtual void SetUp() {
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
    engine_.reset(new MockCommandBufferEngine());
    decoder_.reset(new MockDecoder());
    decoder_->set_engine(engine_.get());
    manager_.reset(new QueryManager());
  }

  virtual void TearDown() {
    decoder_.reset();
    manager_->Destroy(false);
    manager_.reset();
    engine_.reset();
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  scoped_ptr<MockDecoder> decoder_;
  scoped_ptr<QueryManager> manager_;

 private:
  class MockCommandBufferEngine : public CommandBufferEngine {
   public:
    MockCommandBufferEngine() {
      data_.reset(new int8[kSharedBufferSize]);
      ClearSharedMemory();
      valid_buffer_.ptr = data_.get();
      valid_buffer_.size = kSharedBufferSize;
    }

    virtual ~MockCommandBufferEngine() {
    }

    virtual Buffer GetSharedMemoryBuffer(int32 shm_id) OVERRIDE {
      return shm_id == kSharedMemoryId ? valid_buffer_ : invalid_buffer_;
    }

    void ClearSharedMemory() {
      memset(data_.get(), kInitialMemoryValue, kSharedBufferSize);
    }

    virtual void set_token(int32 token) OVERRIDE {
      DCHECK(false);
    }

    virtual bool SetGetBuffer(int32 /* transfer_buffer_id */) OVERRIDE {
      DCHECK(false);
      return false;
    }

    // Overridden from CommandBufferEngine.
    virtual bool SetGetOffset(int32 offset) OVERRIDE {
      DCHECK(false);
      return false;
    }

    // Overridden from CommandBufferEngine.
    virtual int32 GetGetOffset() OVERRIDE {
      DCHECK(false);
      return 0;
    }

   private:
    scoped_array<int8> data_;
    Buffer valid_buffer_;
    Buffer invalid_buffer_;
  };

  scoped_ptr<MockCommandBufferEngine> engine_;
};

// GCC requires these declarations, but MSVC requires they not be present
#ifndef COMPILER_MSVC
const int32 QueryManagerTest::kSharedMemoryId;
const size_t QueryManagerTest::kSharedBufferSize;
const uint32 QueryManagerTest::kSharedMemoryOffset;
const int32 QueryManagerTest::kInvalidSharedMemoryId;
const uint32 QueryManagerTest::kInvalidSharedMemoryOffset;
const uint32 QueryManagerTest::kInitialResult;
const uint8 QueryManagerTest::kInitialMemoryValue;
#endif

TEST_F(QueryManagerTest, Basic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;

  // Check we can create a Query.
  QueryManager::Query::Ref query(
      manager_->CreateQuery(kClient1Id, kService1Id));
  ASSERT_TRUE(query.get() != NULL);
  // Check we can get the same Query.
  EXPECT_EQ(query.get(), manager_->GetQuery(kClient1Id));
  // Check we can get the client id.
  GLuint client_id = -1;
  EXPECT_TRUE(manager_->GetClientId(kService1Id, &client_id));
  EXPECT_EQ(kClient1Id, client_id);
  // Check we get nothing for a non-existent query.
  EXPECT_TRUE(manager_->GetQuery(kClient2Id) == NULL);
  // Check we can delete the query.
  manager_->RemoveQuery(kClient1Id);
  // Check we get nothing for a non-existent query.
  EXPECT_TRUE(manager_->GetQuery(kClient1Id) == NULL);
  // Check query is deleted
  EXPECT_TRUE(query->IsDeleted());
}

TEST_F(QueryManagerTest, Destroy) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;

  // Create Query.
  QueryManager::Query::Ref query(
      manager_->CreateQuery(kClient1Id, kService1Id));
  ASSERT_TRUE(query.get() != NULL);
  EXPECT_CALL(*gl_, DeleteQueriesARB(1, ::testing::Pointee(kService1Id)))
      .Times(1)
      .RetiresOnSaturation();
  manager_->Destroy(true);
  // Check we get nothing for a non-existent query.
  EXPECT_TRUE(manager_->GetQuery(kClient1Id) == NULL);
  // Check query is deleted
  EXPECT_TRUE(query->IsDeleted());
}

TEST_F(QueryManagerTest, QueryBasic) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;

  // Create Query.
  QueryManager::Query::Ref query(
      manager_->CreateQuery(kClient1Id, kService1Id));
  ASSERT_TRUE(query.get() != NULL);

  EXPECT_FALSE(query->IsValid());
  EXPECT_FALSE(query->IsDeleted());
  EXPECT_FALSE(query->IsInitialized());
  EXPECT_FALSE(query->pending());
}

TEST_F(QueryManagerTest, QueryInitialize) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;

  // Create Query.
  QueryManager::Query::Ref query(
      manager_->CreateQuery(kClient1Id, kService1Id));
  ASSERT_TRUE(query.get() != NULL);

  query->Initialize(kTarget, kSharedMemoryId, kSharedMemoryOffset);
  EXPECT_EQ(kTarget, query->target());
  EXPECT_EQ(kSharedMemoryId, query->shm_id());
  EXPECT_EQ(kSharedMemoryOffset, query->shm_offset());

  EXPECT_TRUE(query->IsValid());
  EXPECT_FALSE(query->IsDeleted());
  EXPECT_TRUE(query->IsInitialized());
  EXPECT_FALSE(query->pending());
}

TEST_F(QueryManagerTest, ProcessPendingQuery) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;
  const uint32 kSubmitCount = 123;
  const GLuint kResult = 456;

  // Check nothing happens if there are no pending queries.
  EXPECT_TRUE(manager_->ProcessPendingQueries(decoder_.get()));

  // Create Query.
  QueryManager::Query::Ref query(
      manager_->CreateQuery(kClient1Id, kService1Id));
  ASSERT_TRUE(query.get() != NULL);
  query->Initialize(kTarget, kSharedMemoryId, kSharedMemoryOffset);

  // Setup shared memory like client would.
  QuerySync* sync = decoder_->GetSharedMemoryAs<QuerySync*>(
      kSharedMemoryId, kSharedMemoryOffset, sizeof(*sync));
  ASSERT_TRUE(sync != NULL);
  sync->Reset();

  // Queue it
  manager_->AddPendingQuery(query.get(), kSubmitCount);
  EXPECT_TRUE(query->pending());

  // Process with return not available.
  // Expect 1 GL command.
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgumentPointee<2>(0))
      .RetiresOnSaturation();
  EXPECT_TRUE(manager_->ProcessPendingQueries(decoder_.get()));
  EXPECT_TRUE(query->pending());
  EXPECT_EQ(0u, sync->process_count);
  EXPECT_EQ(0u, sync->result);

  // Process with return available.
  // Expect 2 GL commands.
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgumentPointee<2>(1))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_EXT, _))
      .WillOnce(SetArgumentPointee<2>(kResult))
      .RetiresOnSaturation();
  EXPECT_TRUE(manager_->ProcessPendingQueries(decoder_.get()));
  EXPECT_FALSE(query->pending());
  EXPECT_EQ(kSubmitCount, sync->process_count);
  EXPECT_EQ(kResult, sync->result);

  // Process with no queries.
  // Expect no GL commands/
  EXPECT_TRUE(manager_->ProcessPendingQueries(decoder_.get()));
}

TEST_F(QueryManagerTest, ProcessPendingQueries) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLuint kClient2Id = 2;
  const GLuint kService2Id = 12;
  const GLuint kClient3Id = 3;
  const GLuint kService3Id = 13;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;
  const uint32 kSubmitCount1 = 123;
  const uint32 kSubmitCount2 = 123;
  const uint32 kSubmitCount3 = 123;
  const GLuint kResult1 = 456;
  const GLuint kResult2 = 457;
  const GLuint kResult3 = 458;

  // Create Queries.
  QueryManager::Query::Ref query1(
      manager_->CreateQuery(kClient1Id, kService1Id));
  QueryManager::Query::Ref query2(
      manager_->CreateQuery(kClient2Id, kService2Id));
  QueryManager::Query::Ref query3(
      manager_->CreateQuery(kClient3Id, kService3Id));
  ASSERT_TRUE(query1.get() != NULL);
  ASSERT_TRUE(query2.get() != NULL);
  ASSERT_TRUE(query3.get() != NULL);

  // Setup shared memory like client would.
  QuerySync* sync1 = decoder_->GetSharedMemoryAs<QuerySync*>(
      kSharedMemoryId, kSharedMemoryOffset, sizeof(*sync1) * 3);
  ASSERT_TRUE(sync1 != NULL);
  QuerySync* sync2 = sync1 + 1;
  QuerySync* sync3 = sync2 + 1;

  sync1->Reset();
  sync2->Reset();
  sync3->Reset();

  query1->Initialize(kTarget, kSharedMemoryId, kSharedMemoryOffset);
  query2->Initialize(kTarget, kSharedMemoryId,
                     kSharedMemoryOffset + sizeof(*sync1));
  query3->Initialize(kTarget, kSharedMemoryId,
                     kSharedMemoryOffset + sizeof(*sync1) * 2);

  // Queue them
  manager_->AddPendingQuery(query1.get(), kSubmitCount1);
  manager_->AddPendingQuery(query2.get(), kSubmitCount2);
  manager_->AddPendingQuery(query3.get(), kSubmitCount3);
  EXPECT_TRUE(query1->pending());
  EXPECT_TRUE(query2->pending());
  EXPECT_TRUE(query3->pending());

  // Process with return available for first 2 queries.
  // Expect 4 GL commands.
  {
    InSequence s;
    EXPECT_CALL(*gl_,
        GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
        .WillOnce(SetArgumentPointee<2>(1))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
        GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_EXT, _))
        .WillOnce(SetArgumentPointee<2>(kResult1))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
        GetQueryObjectuivARB(kService2Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
        .WillOnce(SetArgumentPointee<2>(1))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
        GetQueryObjectuivARB(kService2Id, GL_QUERY_RESULT_EXT, _))
        .WillOnce(SetArgumentPointee<2>(kResult2))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
        GetQueryObjectuivARB(kService3Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
        .WillOnce(SetArgumentPointee<2>(0))
        .RetiresOnSaturation();
    EXPECT_TRUE(manager_->ProcessPendingQueries(decoder_.get()));
  }
  EXPECT_FALSE(query1->pending());
  EXPECT_FALSE(query2->pending());
  EXPECT_TRUE(query3->pending());
  EXPECT_EQ(kSubmitCount1, sync1->process_count);
  EXPECT_EQ(kSubmitCount2, sync2->process_count);
  EXPECT_EQ(kResult1, sync1->result);
  EXPECT_EQ(kResult2, sync2->result);
  EXPECT_EQ(0u, sync3->process_count);
  EXPECT_EQ(0u, sync3->result);

  // Process with renaming query. No result.
  // Expect 1 GL commands.
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService3Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgumentPointee<2>(0))
      .RetiresOnSaturation();
  EXPECT_TRUE(manager_->ProcessPendingQueries(decoder_.get()));
  EXPECT_TRUE(query3->pending());
  EXPECT_EQ(0u, sync3->process_count);
  EXPECT_EQ(0u, sync3->result);

  // Process with renaming query. With result.
  // Expect 2 GL commands.
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService3Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgumentPointee<2>(1))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService3Id, GL_QUERY_RESULT_EXT, _))
      .WillOnce(SetArgumentPointee<2>(kResult3))
      .RetiresOnSaturation();
  EXPECT_TRUE(manager_->ProcessPendingQueries(decoder_.get()));
  EXPECT_FALSE(query3->pending());
  EXPECT_EQ(kSubmitCount3, sync3->process_count);
  EXPECT_EQ(kResult3, sync3->result);
}

TEST_F(QueryManagerTest, ProcessPendingBadSharedMemoryId) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;
  const uint32 kSubmitCount = 123;
  const GLuint kResult = 456;

  // Create Query.
  QueryManager::Query::Ref query(
      manager_->CreateQuery(kClient1Id, kService1Id));
  ASSERT_TRUE(query.get() != NULL);
  query->Initialize(kTarget, kInvalidSharedMemoryId, kSharedMemoryOffset);

  // Queue it
  manager_->AddPendingQuery(query.get(), kSubmitCount);

  // Process with return available.
  // Expect 2 GL commands.
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgumentPointee<2>(1))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_EXT, _))
      .WillOnce(SetArgumentPointee<2>(kResult))
      .RetiresOnSaturation();
  EXPECT_FALSE(manager_->ProcessPendingQueries(decoder_.get()));
}

TEST_F(QueryManagerTest, ProcessPendingBadSharedMemoryOffset) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;
  const uint32 kSubmitCount = 123;
  const GLuint kResult = 456;

  // Create Query.
  QueryManager::Query::Ref query(
      manager_->CreateQuery(kClient1Id, kService1Id));
  ASSERT_TRUE(query.get() != NULL);
  query->Initialize(kTarget, kSharedMemoryId, kInvalidSharedMemoryOffset);

  // Queue it
  manager_->AddPendingQuery(query.get(), kSubmitCount);

  // Process with return available.
  // Expect 2 GL commands.
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_AVAILABLE_EXT, _))
      .WillOnce(SetArgumentPointee<2>(1))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_,
      GetQueryObjectuivARB(kService1Id, GL_QUERY_RESULT_EXT, _))
      .WillOnce(SetArgumentPointee<2>(kResult))
      .RetiresOnSaturation();
  EXPECT_FALSE(manager_->ProcessPendingQueries(decoder_.get()));
}

TEST_F(QueryManagerTest, ExitWithPendingQuery) {
  const GLuint kClient1Id = 1;
  const GLuint kService1Id = 11;
  const GLenum kTarget = GL_ANY_SAMPLES_PASSED_EXT;
  const uint32 kSubmitCount = 123;

  // Create Query.
  QueryManager::Query::Ref query(
      manager_->CreateQuery(kClient1Id, kService1Id));
  ASSERT_TRUE(query.get() != NULL);
  query->Initialize(kTarget, kSharedMemoryId, kSharedMemoryOffset);

  // Queue it
  manager_->AddPendingQuery(query.get(), kSubmitCount);
}

}  // namespace gles2
}  // namespace gpu


