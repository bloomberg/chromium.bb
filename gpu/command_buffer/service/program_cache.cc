// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/program_cache.h"

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/service/shader_manager.h"

namespace gpu {
namespace gles2 {

ProgramCache::ProgramCache() {}
ProgramCache::~ProgramCache() {}

void ProgramCache::Clear() {
  shader_status_.clear();
  link_status_.clear();
  ClearBackend();
}

ProgramCache::CompiledShaderStatus ProgramCache::GetShaderCompilationStatus(
    const std::string& shader_src) const {
  char sha[kHashLength];
  ComputeShaderHash(shader_src, sha);
  const std::string sha_string(sha, kHashLength);

  CompileStatusMap::const_iterator found = shader_status_.find(sha_string);

  if (found == shader_status_.end()) {
    return ProgramCache::COMPILATION_UNKNOWN;
  } else {
    return found->second.status;
  }
}

void ProgramCache::ShaderCompilationSucceeded(
    const std::string& shader_src) {
  char sha[kHashLength];
  ComputeShaderHash(shader_src, sha);
  const std::string sha_string(sha, kHashLength);

  CompileStatusMap::iterator it = shader_status_.find(sha_string);
  if (it == shader_status_.end()) {
    shader_status_[sha_string] = CompiledShaderInfo(COMPILATION_SUCCEEDED);
  } else {
    it->second.status = COMPILATION_SUCCEEDED;
  }
}

ProgramCache::LinkedProgramStatus ProgramCache::GetLinkedProgramStatus(
    const std::string& untranslated_a,
    const std::string& untranslated_b,
    const std::map<std::string, GLint>* bind_attrib_location_map) const {
  char a_sha[kHashLength];
  char b_sha[kHashLength];
  ComputeShaderHash(untranslated_a, a_sha);
  ComputeShaderHash(untranslated_b, b_sha);

  char sha[kHashLength];
  ComputeProgramHash(a_sha,
                     b_sha,
                     bind_attrib_location_map,
                     sha);
  const std::string sha_string(sha, kHashLength);

  LinkStatusMap::const_iterator found = link_status_.find(sha_string);
  if (found == link_status_.end()) {
    return ProgramCache::LINK_UNKNOWN;
  } else {
    return found->second;
  }
}

void ProgramCache::LinkedProgramCacheSuccess(
    const std::string& shader_a,
    const std::string& shader_b,
    const LocationMap* bind_attrib_location_map) {
  char a_sha[kHashLength];
  char b_sha[kHashLength];
  ComputeShaderHash(shader_a, a_sha);
  ComputeShaderHash(shader_b, b_sha);
  char sha[kHashLength];
  ComputeProgramHash(a_sha,
                     b_sha,
                     bind_attrib_location_map,
                     sha);
  const std::string sha_string(sha, kHashLength);

  LinkedProgramCacheSuccess(sha_string,
                            std::string(a_sha, kHashLength),
                            std::string(b_sha, kHashLength));
}

void ProgramCache::LinkedProgramCacheSuccess(const std::string& program_hash,
                                             const std::string& shader_a_hash,
                                             const std::string& shader_b_hash) {
  link_status_[program_hash] = LINK_SUCCEEDED;
  shader_status_[shader_a_hash].ref_count++;
  shader_status_[shader_b_hash].ref_count++;
}

void ProgramCache::ComputeShaderHash(const std::string& str,
                                     char* result) const {
  base::SHA1HashBytes(reinterpret_cast<const unsigned char*>(str.c_str()),
                      str.length(), reinterpret_cast<unsigned char*>(result));
}

void ProgramCache::Evict(const std::string& program_hash,
                         const std::string& shader_0_hash,
                         const std::string& shader_1_hash) {
  CompileStatusMap::iterator info0 = shader_status_.find(shader_0_hash);
  CompileStatusMap::iterator info1 = shader_status_.find(shader_1_hash);
  DCHECK(info0 != shader_status_.end());
  DCHECK(info1 != shader_status_.end());
  DCHECK(info0->second.ref_count > 0);
  DCHECK(info1->second.ref_count > 0);
  if (--info0->second.ref_count <= 0) {
    shader_status_.erase(shader_0_hash);
  }
  if (--info1->second.ref_count <= 0) {
    shader_status_.erase(shader_1_hash);
  }
  link_status_.erase(program_hash);
}

namespace {
size_t CalculateMapSize(const std::map<std::string, GLint>* map) {
  if (!map) {
    return 0;
  }
  std::map<std::string, GLint>::const_iterator it;
  size_t total = 0;
  for (it = map->begin(); it != map->end(); ++it) {
    total += 4 + it->first.length();
  }
  return total;
}
}  // anonymous namespace

void ProgramCache::ComputeProgramHash(
    const char* hashed_shader_0,
    const char* hashed_shader_1,
    const std::map<std::string, GLint>* bind_attrib_location_map,
    char* result) const {
  const size_t shader0_size = kHashLength;
  const size_t shader1_size = kHashLength;
  const size_t map_size = CalculateMapSize(bind_attrib_location_map);
  const size_t total_size = shader0_size + shader1_size + map_size;

  scoped_array<unsigned char> buffer(new unsigned char[total_size]);
  memcpy(buffer.get(), hashed_shader_0, shader0_size);
  memcpy(&buffer[shader0_size], hashed_shader_1, shader1_size);
  if (map_size != 0) {
    // copy our map
    size_t current_pos = shader0_size + shader1_size;
    std::map<std::string, GLint>::const_iterator it;
    for (it = bind_attrib_location_map->begin();
         it != bind_attrib_location_map->end();
         ++it) {
      const size_t name_size = it->first.length();
      memcpy(&buffer.get()[current_pos], it->first.c_str(), name_size);
      current_pos += name_size;
      const GLint value = it->second;
      buffer[current_pos++] = value >> 24;
      buffer[current_pos++] = value >> 16;
      buffer[current_pos++] = value >> 8;
      buffer[current_pos++] = value;
    }
  }
  base::SHA1HashBytes(buffer.get(),
                      total_size, reinterpret_cast<unsigned char*>(result));
}

}  // namespace gles2
}  // namespace gpu
