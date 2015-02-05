// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/program_info_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

uint32 ComputeOffset(const void* start, const void* position) {
  return static_cast<const uint8*>(position) -
         static_cast<const uint8*>(start);
}

const GLuint kClientProgramId = 321;

}  // namespace anonymous

namespace gpu {
namespace gles2 {

class ProgramInfoManagerTest : public testing::Test {
 public:
  ProgramInfoManagerTest() {}
  ~ProgramInfoManagerTest() override {}

 protected:
  typedef ProgramInfoManager::Program Program;

  struct UniformBlocksData {
    UniformBlocksHeader header;
    UniformBlockInfo entry[2];
    char name0[4];
    uint32_t indices0[2];
    char name1[8];
    uint32_t indices1[1];
  };

  void SetUp() override {
    program_info_manager_.reset(new ProgramInfoManager);
    program_info_manager_->CreateInfo(kClientProgramId);
    {
      base::AutoLock auto_lock(program_info_manager_->lock_);
      program_ = program_info_manager_->GetProgramInfo(
          NULL, kClientProgramId, ProgramInfoManager::kNone);
      ASSERT_TRUE(program_ != NULL);
    }
  }

  void TearDown() override {}

  void SetupUniformBlocksData(UniformBlocksData* data) {
    // The names needs to be of size 4*k-1 to avoid padding in the struct Data.
    // This is a testing only problem.
    const char* kName[] = { "cow", "chicken" };
    const uint32_t kIndices0[] = { 1, 2 };
    const uint32_t kIndices1[] = { 3 };
    const uint32_t* kIndices[] = { kIndices0, kIndices1 };
    data->header.num_uniform_blocks = 2;
    data->entry[0].binding = 0;
    data->entry[0].data_size = 8;
    data->entry[0].name_offset = ComputeOffset(data, data->name0);
    data->entry[0].name_length = arraysize(data->name0);
    data->entry[0].active_uniforms = arraysize(data->indices0);
    data->entry[0].active_uniform_offset = ComputeOffset(data, data->indices0);
    data->entry[0].referenced_by_vertex_shader = static_cast<uint32_t>(true);
    data->entry[0].referenced_by_fragment_shader = static_cast<uint32_t>(false);
    data->entry[1].binding = 1;
    data->entry[1].data_size = 4;
    data->entry[1].name_offset = ComputeOffset(data, data->name1);
    data->entry[1].name_length = arraysize(data->name1);
    data->entry[1].active_uniforms = arraysize(data->indices1);
    data->entry[1].active_uniform_offset = ComputeOffset(data, data->indices1);
    data->entry[1].referenced_by_vertex_shader = static_cast<uint32_t>(false);
    data->entry[1].referenced_by_fragment_shader = static_cast<uint32_t>(true);
    memcpy(data->name0, kName[0], arraysize(data->name0));
    data->indices0[0] = kIndices[0][0];
    data->indices0[1] = kIndices[0][1];
    memcpy(data->name1, kName[1], arraysize(data->name1));
    data->indices1[0] = kIndices[1][0];
  }

  scoped_ptr<ProgramInfoManager> program_info_manager_;
  Program* program_;
};

TEST_F(ProgramInfoManagerTest, UpdateES3UniformBlocks) {
  UniformBlocksData data;
  SetupUniformBlocksData(&data);
  const std::string kName[] = { data.name0, data.name1 };
  const uint32_t* kIndices[] = { data.indices0, data.indices1 };
  std::vector<int8> result(sizeof(data));
  memcpy(&result[0], &data, sizeof(data));
  EXPECT_FALSE(program_->IsCached(ProgramInfoManager::kES3UniformBlocks));
  program_->UpdateES3UniformBlocks(result);
  EXPECT_TRUE(program_->IsCached(ProgramInfoManager::kES3UniformBlocks));

  GLint uniform_block_count = 0;
  EXPECT_TRUE(program_->GetProgramiv(
      GL_ACTIVE_UNIFORM_BLOCKS, &uniform_block_count));
  EXPECT_EQ(data.header.num_uniform_blocks,
            static_cast<uint32_t>(uniform_block_count));
  GLint max_name_length = 0;
  EXPECT_TRUE(program_->GetProgramiv(
      GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &max_name_length));
  for (uint32_t ii = 0; ii < data.header.num_uniform_blocks; ++ii) {
    EXPECT_EQ(ii, program_->GetUniformBlockIndex(kName[ii]));
    const Program::UniformBlock* info = program_->GetUniformBlock(ii);
    EXPECT_TRUE(info != NULL);
    EXPECT_EQ(data.entry[ii].binding, info->binding);
    EXPECT_EQ(data.entry[ii].data_size, info->data_size);
    EXPECT_EQ(data.entry[ii].active_uniforms,
              info->active_uniform_indices.size());
    for (uint32_t uu = 0; uu < data.entry[ii].active_uniforms; ++uu) {
      EXPECT_EQ(kIndices[ii][uu], info->active_uniform_indices[uu]);
    }
    EXPECT_EQ(data.entry[ii].referenced_by_vertex_shader,
              static_cast<GLboolean>(info->referenced_by_vertex_shader));
    EXPECT_EQ(data.entry[ii].referenced_by_fragment_shader,
              static_cast<GLboolean>(info->referenced_by_fragment_shader));
    EXPECT_EQ(kName[ii], info->name);
    EXPECT_GE(max_name_length, static_cast<GLint>(info->name.size()) + 1);
  }

  EXPECT_EQ(GL_INVALID_INDEX, program_->GetUniformBlockIndex("BadName"));
  EXPECT_EQ(NULL, program_->GetUniformBlock(data.header.num_uniform_blocks));
}

TEST_F(ProgramInfoManagerTest, GetUniformBlockIndexCached) {
  UniformBlocksData data;
  SetupUniformBlocksData(&data);
  std::vector<int8> result(sizeof(data));
  memcpy(&result[0], &data, sizeof(data));
  program_->UpdateES3UniformBlocks(result);

  EXPECT_EQ(0u, program_info_manager_->GetUniformBlockIndex(
      NULL, kClientProgramId, data.name0));
  EXPECT_EQ(1u, program_info_manager_->GetUniformBlockIndex(
      NULL, kClientProgramId, data.name1));
  EXPECT_EQ(GL_INVALID_INDEX, program_info_manager_->GetUniformBlockIndex(
      NULL, kClientProgramId, "BadName"));
}

TEST_F(ProgramInfoManagerTest, GetActiveUniformBlockNameCached) {
  UniformBlocksData data;
  SetupUniformBlocksData(&data);
  std::vector<int8> result(sizeof(data));
  memcpy(&result[0], &data, sizeof(data));
  program_->UpdateES3UniformBlocks(result);

  GLsizei buf_size = std::max(strlen(data.name0), strlen(data.name1)) + 1;
  std::vector<char> buffer(buf_size);
  GLsizei length = 0;
  EXPECT_EQ(true, program_info_manager_->GetActiveUniformBlockName(
      NULL, kClientProgramId, 0, buf_size, &length, &buffer[0]));
  EXPECT_EQ(static_cast<GLsizei>(strlen(data.name0)), length);
  EXPECT_STREQ(data.name0, &buffer[0]);

  EXPECT_EQ(true, program_info_manager_->GetActiveUniformBlockName(
      NULL, kClientProgramId, 1, buf_size, &length, &buffer[0]));
  EXPECT_EQ(static_cast<GLsizei>(strlen(data.name1)), length);
  EXPECT_STREQ(data.name1, &buffer[0]);

  // Test length == NULL.
  EXPECT_EQ(true, program_info_manager_->GetActiveUniformBlockName(
      NULL, kClientProgramId, 0, buf_size, NULL, &buffer[0]));
  EXPECT_STREQ(data.name0, &buffer[0]);

  // Test buffer == NULL.
  EXPECT_EQ(true, program_info_manager_->GetActiveUniformBlockName(
      NULL, kClientProgramId, 0, buf_size, &length, NULL));
  EXPECT_EQ(0, length);

  // Test buf_size smaller than string size.
  buf_size = strlen(data.name0);
  EXPECT_EQ(true, program_info_manager_->GetActiveUniformBlockName(
      NULL, kClientProgramId, 0, buf_size, &length, &buffer[0]));
  EXPECT_EQ(buf_size, length + 1);
  EXPECT_STREQ(std::string(data.name0).substr(0, length).c_str(), &buffer[0]);
}

}  // namespace gles2
}  // namespace gpu

