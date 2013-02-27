// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/memory_program_cache.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/sha1.h"
#include "base/string_number_conversions.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/gl/gl_bindings.h"

namespace {
size_t GetCacheSizeBytes() {
  size_t size;
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kGpuProgramCacheSizeKb) &&
      base::StringToSizeT(command_line->GetSwitchValueNative(
          switches::kGpuProgramCacheSizeKb),
          &size)) {
      return size * 1024;
  }
  return gpu::gles2::MemoryProgramCache::kDefaultMaxProgramCacheMemoryBytes;
}
}  // anonymous namespace

namespace gpu {
namespace gles2 {

MemoryProgramCache::MemoryProgramCache()
    : max_size_bytes_(GetCacheSizeBytes()),
      curr_size_bytes_(0) { }

MemoryProgramCache::MemoryProgramCache(const size_t max_cache_size_bytes)
    : max_size_bytes_(max_cache_size_bytes),
      curr_size_bytes_(0) {}

MemoryProgramCache::~MemoryProgramCache() {}

void MemoryProgramCache::ClearBackend() {
  curr_size_bytes_ = 0;
  store_.clear();
  eviction_helper_.Clear();
}

ProgramCache::ProgramLoadResult MemoryProgramCache::LoadLinkedProgram(
    GLuint program,
    Shader* shader_a,
    Shader* shader_b,
    const LocationMap* bind_attrib_location_map) const {
  char a_sha[kHashLength];
  char b_sha[kHashLength];
  ComputeShaderHash(*shader_a->deferred_compilation_source(), a_sha);
  ComputeShaderHash(*shader_b->deferred_compilation_source(), b_sha);

  char sha[kHashLength];
  ComputeProgramHash(a_sha,
                     b_sha,
                     bind_attrib_location_map,
                     sha);
  const std::string sha_string(sha, kHashLength);

  StoreMap::const_iterator found = store_.find(sha_string);
  if (found == store_.end()) {
    return PROGRAM_LOAD_FAILURE;
  }
  const scoped_refptr<ProgramCacheValue> value = found->second;
  glProgramBinary(program,
                  value->format,
                  static_cast<const GLvoid*>(value->data.get()),
                  value->length);
  GLint success = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (success == GL_FALSE) {
    return PROGRAM_LOAD_FAILURE;
  }
  shader_a->set_attrib_map(value->attrib_map_0);
  shader_a->set_uniform_map(value->uniform_map_0);
  shader_b->set_attrib_map(value->attrib_map_1);
  shader_b->set_uniform_map(value->uniform_map_1);
  return PROGRAM_LOAD_SUCCESS;
}

void MemoryProgramCache::SaveLinkedProgram(
    GLuint program,
    const Shader* shader_a,
    const Shader* shader_b,
    const LocationMap* bind_attrib_location_map) {
  GLenum format;
  GLsizei length = 0;
  glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH_OES, &length);
  if (length == 0 || static_cast<unsigned int>(length) > max_size_bytes_) {
    return;
  }
  scoped_array<char> binary(new char[length]);
  glGetProgramBinary(program,
                     length,
                     NULL,
                     &format,
                     binary.get());
  UMA_HISTOGRAM_COUNTS("GPU.ProgramCache.ProgramBinarySizeBytes", length);

  char a_sha[kHashLength];
  char b_sha[kHashLength];
  ComputeShaderHash(*shader_a->deferred_compilation_source(), a_sha);
  ComputeShaderHash(*shader_b->deferred_compilation_source(), b_sha);

  char sha[kHashLength];
  ComputeProgramHash(a_sha,
                     b_sha,
                     bind_attrib_location_map,
                     sha);
  const std::string sha_string(sha, sizeof(sha));

  UMA_HISTOGRAM_COUNTS("GPU.ProgramCache.MemorySizeBeforeKb",
                       curr_size_bytes_ / 1024);

  if (store_.find(sha_string) != store_.end()) {
    const StoreMap::iterator found = store_.find(sha_string);
    const ProgramCacheValue* evicting = found->second;
    curr_size_bytes_ -= evicting->length;
    Evict(sha_string, evicting->shader_0_hash, evicting->shader_1_hash);
    store_.erase(found);
  }

  while (curr_size_bytes_ + length > max_size_bytes_) {
    DCHECK(!eviction_helper_.IsEmpty());
    const std::string* program = eviction_helper_.PeekKey();
    const StoreMap::iterator found = store_.find(*program);
    const ProgramCacheValue* evicting = found->second.get();
    curr_size_bytes_ -= evicting->length;
    Evict(*program, evicting->shader_0_hash, evicting->shader_1_hash);
    store_.erase(found);
    eviction_helper_.PopKey();
  }
  store_[sha_string] = new ProgramCacheValue(length,
                                             format,
                                             binary.release(),
                                             a_sha,
                                             shader_a->attrib_map(),
                                             shader_a->uniform_map(),
                                             b_sha,
                                             shader_b->attrib_map(),
                                             shader_b->uniform_map());
  curr_size_bytes_ += length;
  eviction_helper_.KeyUsed(sha_string);

  UMA_HISTOGRAM_COUNTS("GPU.ProgramCache.MemorySizeAfterKb",
                         curr_size_bytes_ / 1024);

  LinkedProgramCacheSuccess(sha_string,
                            std::string(a_sha, kHashLength),
                            std::string(b_sha, kHashLength));
}

MemoryProgramCache::ProgramCacheValue::ProgramCacheValue(
    GLsizei _length,
    GLenum _format,
    const char* _data,
    const char* _shader_0_hash,
    const ShaderTranslator::VariableMap& _attrib_map_0,
    const ShaderTranslator::VariableMap& _uniform_map_0,
    const char* _shader_1_hash,
    const ShaderTranslator::VariableMap& _attrib_map_1,
    const ShaderTranslator::VariableMap& _uniform_map_1)
    : length(_length),
      format(_format),
      data(_data),
      shader_0_hash(_shader_0_hash, kHashLength),
      attrib_map_0(_attrib_map_0),
      uniform_map_0(_uniform_map_0),
      shader_1_hash(_shader_1_hash, kHashLength),
      attrib_map_1(_attrib_map_1),
      uniform_map_1(_uniform_map_1) {}

MemoryProgramCache::ProgramCacheValue::~ProgramCacheValue() {}

}  // namespace gles2
}  // namespace gpu
