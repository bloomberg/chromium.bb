// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MEMORY_PROGRAM_CACHE_H_
#define GPU_COMMAND_BUFFER_SERVICE_MEMORY_PROGRAM_CACHE_H_

#include <map>
#include <string>

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/service/program_cache.h"
#include "gpu/command_buffer/service/program_cache_lru_helper.h"
#include "gpu/command_buffer/service/shader_translator.h"

namespace gpu {
namespace gles2 {

// Program cache that stores binaries completely in-memory
class GPU_EXPORT MemoryProgramCache : public ProgramCache {
 public:
  static const size_t kDefaultMaxProgramCacheMemoryBytes = 6 * 1024 * 1024;

  MemoryProgramCache();
  explicit MemoryProgramCache(const size_t max_cache_size_bytes);
  virtual ~MemoryProgramCache();

  virtual ProgramLoadResult LoadLinkedProgram(
      GLuint program,
      Shader* shader_a,
      Shader* shader_b,
      const LocationMap* bind_attrib_location_map) const OVERRIDE;
  virtual void SaveLinkedProgram(
      GLuint program,
      const Shader* shader_a,
      const Shader* shader_b,
      const LocationMap* bind_attrib_location_map) OVERRIDE;

 private:
  virtual void ClearBackend() OVERRIDE;

  struct ProgramCacheValue : public base::RefCounted<ProgramCacheValue> {
   public:
    ProgramCacheValue(GLsizei _length,
                      GLenum _format,
                      const char* _data,
                      const char* _shader_0_hash,
                      const ShaderTranslator::VariableMap& _attrib_map_0,
                      const ShaderTranslator::VariableMap& _uniform_map_0,
                      const char* _shader_1_hash,
                      const ShaderTranslator::VariableMap& _attrib_map_1,
                      const ShaderTranslator::VariableMap& _uniform_map_1);
    const GLsizei length;
    const GLenum format;
    const scoped_array<const char> data;
    const std::string shader_0_hash;
    const ShaderTranslator::VariableMap attrib_map_0;
    const ShaderTranslator::VariableMap uniform_map_0;
    const std::string shader_1_hash;
    const ShaderTranslator::VariableMap attrib_map_1;
    const ShaderTranslator::VariableMap uniform_map_1;

   private:
    friend class base::RefCounted<ProgramCacheValue>;

    ~ProgramCacheValue();

    DISALLOW_COPY_AND_ASSIGN(ProgramCacheValue);
  };

  typedef base::hash_map<std::string,
                         scoped_refptr<ProgramCacheValue> > StoreMap;

  const size_t max_size_bytes_;
  size_t curr_size_bytes_;
  StoreMap store_;
  ProgramCacheLruHelper eviction_helper_;

  DISALLOW_COPY_AND_ASSIGN(MemoryProgramCache);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MEMORY_PROGRAM_CACHE_H_
