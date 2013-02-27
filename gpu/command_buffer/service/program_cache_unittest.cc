// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_cache.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class NoBackendProgramCache : public ProgramCache {
 public:
  virtual ProgramLoadResult LoadLinkedProgram(
      GLuint /* program */,
      Shader* /* shader_a */,
      Shader* /* shader_b */,
      const LocationMap* /* bind_attrib_location_map */) const OVERRIDE {
    return PROGRAM_LOAD_SUCCESS;
  }
  virtual void SaveLinkedProgram(
      GLuint /* program */,
      const Shader* /* shader_a */,
      const Shader* /* shader_b */,
      const LocationMap* /* bind_attrib_location_map */) OVERRIDE { }

  virtual void ClearBackend() OVERRIDE {}

  void SaySuccessfullyCached(const std::string& shader1,
                             const std::string& shader2,
                             std::map<std::string, GLint>* attrib_map) {
    char a_sha[kHashLength];
    char b_sha[kHashLength];
    ComputeShaderHash(shader1, a_sha);
    ComputeShaderHash(shader2, b_sha);

    char sha[kHashLength];
    ComputeProgramHash(a_sha,
                       b_sha,
                       attrib_map,
                       sha);
    const std::string shaString(sha, kHashLength);

    LinkedProgramCacheSuccess(shaString,
                              std::string(a_sha, kHashLength),
                              std::string(b_sha, kHashLength));
  }

  void ComputeShaderHash(const std::string& shader,
                         char* result) const {
    ProgramCache::ComputeShaderHash(shader, result);
  }

  void ComputeProgramHash(const char* hashed_shader_0,
                          const char* hashed_shader_1,
                          const LocationMap* bind_attrib_location_map,
                          char* result) const {
    ProgramCache::ComputeProgramHash(hashed_shader_0,
                                     hashed_shader_1,
                                     bind_attrib_location_map,
                                     result);
  }

  void Evict(const std::string& program_hash,
             const std::string& shader_0_hash,
             const std::string& shader_1_hash) {
    ProgramCache::Evict(program_hash, shader_0_hash, shader_1_hash);
  }
};

class ProgramCacheTest : public testing::Test {
 public:
  ProgramCacheTest() :
    cache_(new NoBackendProgramCache()) { }

 protected:
  scoped_ptr<NoBackendProgramCache> cache_;
};

TEST_F(ProgramCacheTest, CompilationStatusSave) {
  const std::string shader1 = "abcd1234";
  {
    std::string shader = shader1;
    EXPECT_EQ(ProgramCache::COMPILATION_UNKNOWN,
              cache_->GetShaderCompilationStatus(shader));
    cache_->ShaderCompilationSucceeded(shader);
    shader.clear();
  }
  // make sure it was copied
  EXPECT_EQ(ProgramCache::COMPILATION_SUCCEEDED,
            cache_->GetShaderCompilationStatus(shader1));
}

TEST_F(ProgramCacheTest, CompilationUnknownOnSourceChange) {
  std::string shader1 = "abcd1234";
  cache_->ShaderCompilationSucceeded(shader1);

  shader1 = "different!";
  EXPECT_EQ(ProgramCache::COMPILATION_UNKNOWN,
            cache_->GetShaderCompilationStatus(shader1));
}

TEST_F(ProgramCacheTest, LinkStatusSave) {
  const std::string shader1 = "abcd1234";
  const std::string shader2 = "abcda sda b1~#4 bbbbb1234";
  {
    std::string shader_a = shader1;
    std::string shader_b = shader2;
    EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
              cache_->GetLinkedProgramStatus(shader_a, shader_b, NULL));
    cache_->SaySuccessfullyCached(shader_a, shader_b, NULL);

    shader_a.clear();
    shader_b.clear();
  }
  // make sure it was copied
  EXPECT_EQ(ProgramCache::LINK_SUCCEEDED,
            cache_->GetLinkedProgramStatus(shader1, shader2, NULL));
}

TEST_F(ProgramCacheTest, LinkUnknownOnFragmentSourceChange) {
  const std::string shader1 = "abcd1234";
  std::string shader2 = "abcda sda b1~#4 bbbbb1234";
  cache_->SaySuccessfullyCached(shader1, shader2, NULL);

  shader2 = "different!";
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader2, NULL));
}

TEST_F(ProgramCacheTest, LinkUnknownOnVertexSourceChange) {
  std::string shader1 = "abcd1234";
  const std::string shader2 = "abcda sda b1~#4 bbbbb1234";
  cache_->SaySuccessfullyCached(shader1, shader2, NULL);

  shader1 = "different!";
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader2, NULL));
}

TEST_F(ProgramCacheTest, StatusEviction) {
  const std::string shader1 = "abcd1234";
  const std::string shader2 = "abcda sda b1~#4 bbbbb1234";
  cache_->ShaderCompilationSucceeded(shader1);
  cache_->ShaderCompilationSucceeded(shader2);
  cache_->SaySuccessfullyCached(shader1, shader2, NULL);
  char a_sha[ProgramCache::kHashLength];
  char b_sha[ProgramCache::kHashLength];
  cache_->ComputeShaderHash(shader1, a_sha);
  cache_->ComputeShaderHash(shader2, b_sha);

  char sha[ProgramCache::kHashLength];
  cache_->ComputeProgramHash(a_sha,
                             b_sha,
                             NULL,
                             sha);
  cache_->Evict(std::string(sha, ProgramCache::kHashLength),
                std::string(a_sha, ProgramCache::kHashLength),
                std::string(b_sha, ProgramCache::kHashLength));
  EXPECT_EQ(ProgramCache::COMPILATION_UNKNOWN,
            cache_->GetShaderCompilationStatus(shader1));
  EXPECT_EQ(ProgramCache::COMPILATION_UNKNOWN,
            cache_->GetShaderCompilationStatus(shader2));
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader2, NULL));
}

TEST_F(ProgramCacheTest, EvictionWithReusedShader) {
  const std::string shader1 = "abcd1234";
  const std::string shader2 = "abcda sda b1~#4 bbbbb1234";
  const std::string shader3 = "asbjbbjj239a";
  cache_->ShaderCompilationSucceeded(shader1);
  cache_->ShaderCompilationSucceeded(shader2);
  cache_->SaySuccessfullyCached(shader1, shader2, NULL);
  cache_->ShaderCompilationSucceeded(shader1);
  cache_->ShaderCompilationSucceeded(shader3);
  cache_->SaySuccessfullyCached(shader1, shader3, NULL);

  char a_sha[ProgramCache::kHashLength];
  char b_sha[ProgramCache::kHashLength];
  char c_sha[ProgramCache::kHashLength];
  cache_->ComputeShaderHash(shader1, a_sha);
  cache_->ComputeShaderHash(shader2, b_sha);
  cache_->ComputeShaderHash(shader3, c_sha);

  char sha[ProgramCache::kHashLength];
  cache_->ComputeProgramHash(a_sha,
                             b_sha,
                             NULL,
                             sha);
  cache_->Evict(std::string(sha, ProgramCache::kHashLength),
                std::string(a_sha, ProgramCache::kHashLength),
                std::string(b_sha, ProgramCache::kHashLength));
  EXPECT_EQ(ProgramCache::COMPILATION_SUCCEEDED,
            cache_->GetShaderCompilationStatus(shader1));
  EXPECT_EQ(ProgramCache::COMPILATION_UNKNOWN,
            cache_->GetShaderCompilationStatus(shader2));
  EXPECT_EQ(ProgramCache::COMPILATION_SUCCEEDED,
            cache_->GetShaderCompilationStatus(shader3));
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader2, NULL));
  EXPECT_EQ(ProgramCache::LINK_SUCCEEDED,
            cache_->GetLinkedProgramStatus(shader1, shader3, NULL));


  cache_->ComputeProgramHash(a_sha,
                             c_sha,
                             NULL,
                             sha);
  cache_->Evict(std::string(sha, ProgramCache::kHashLength),
                std::string(a_sha, ProgramCache::kHashLength),
                std::string(c_sha, ProgramCache::kHashLength));
  EXPECT_EQ(ProgramCache::COMPILATION_UNKNOWN,
            cache_->GetShaderCompilationStatus(shader1));
  EXPECT_EQ(ProgramCache::COMPILATION_UNKNOWN,
            cache_->GetShaderCompilationStatus(shader2));
  EXPECT_EQ(ProgramCache::COMPILATION_UNKNOWN,
            cache_->GetShaderCompilationStatus(shader3));
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader2, NULL));
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader3, NULL));
}

TEST_F(ProgramCacheTest, StatusClear) {
  const std::string shader1 = "abcd1234";
  const std::string shader2 = "abcda sda b1~#4 bbbbb1234";
  const std::string shader3 = "asbjbbjj239a";
  cache_->ShaderCompilationSucceeded(shader1);
  cache_->ShaderCompilationSucceeded(shader2);
  cache_->SaySuccessfullyCached(shader1, shader2, NULL);
  cache_->ShaderCompilationSucceeded(shader3);
  cache_->SaySuccessfullyCached(shader1, shader3, NULL);
  cache_->Clear();
  EXPECT_EQ(ProgramCache::COMPILATION_UNKNOWN,
            cache_->GetShaderCompilationStatus(shader1));
  EXPECT_EQ(ProgramCache::COMPILATION_UNKNOWN,
            cache_->GetShaderCompilationStatus(shader2));
  EXPECT_EQ(ProgramCache::COMPILATION_UNKNOWN,
            cache_->GetShaderCompilationStatus(shader3));
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader2, NULL));
  EXPECT_EQ(ProgramCache::LINK_UNKNOWN,
            cache_->GetLinkedProgramStatus(shader1, shader3, NULL));
}

}  // namespace gles2
}  // namespace gpu
