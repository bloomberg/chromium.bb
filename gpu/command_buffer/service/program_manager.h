// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_PROGRAM_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_PROGRAM_MANAGER_H_

#include <map>
#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/service/common_decoder.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shader_manager.h"
#include "gpu/gpu_export.h"

namespace gpu {
namespace gles2 {

// Tracks the Programs.
//
// NOTE: To support shared resources an instance of this class will
// need to be shared by multiple GLES2Decoders.
class GPU_EXPORT ProgramManager {
 public:
  // This is used to track which attributes a particular program needs
  // so we can verify at glDrawXXX time that every attribute is either disabled
  // or if enabled that it points to a valid source.
  class GPU_EXPORT ProgramInfo : public base::RefCounted<ProgramInfo> {
   public:
    typedef scoped_refptr<ProgramInfo> Ref;

    static const int kMaxAttachedShaders = 2;

    struct UniformInfo {
      UniformInfo(
          GLsizei _size, GLenum _type, GLint _fake_location_base,
          const std::string& _name);
      ~UniformInfo();

      bool IsSampler() const {
        return type == GL_SAMPLER_2D || type == GL_SAMPLER_CUBE ||
               type == GL_SAMPLER_EXTERNAL_OES;
      }

      GLsizei size;
      GLenum type;
      GLint fake_location_base;
      bool is_array;
      std::string name;
      std::vector<GLint> element_locations;
      std::vector<GLuint> texture_units;
    };
    struct VertexAttribInfo {
      VertexAttribInfo(GLsizei _size, GLenum _type, const std::string& _name,
                       GLint _location)
          : size(_size),
            type(_type),
            location(_location),
            name(_name) {
      }
      GLsizei size;
      GLenum type;
      GLint location;
      std::string name;
    };

    typedef std::vector<UniformInfo> UniformInfoVector;
    typedef std::vector<VertexAttribInfo> AttribInfoVector;
    typedef std::vector<int> SamplerIndices;

    ProgramInfo(ProgramManager* manager, GLuint service_id);

    GLuint service_id() const {
      return service_id_;
    }

    const SamplerIndices& sampler_indices() {
      return sampler_indices_;
    }

    const AttribInfoVector& GetAttribInfos() const {
      return attrib_infos_;
    }

    const VertexAttribInfo* GetAttribInfo(GLint index) const {
      return (static_cast<size_t>(index) < attrib_infos_.size()) ?
         &attrib_infos_[index] : NULL;
    }

    GLint GetAttribLocation(const std::string& name) const;

    const VertexAttribInfo* GetAttribInfoByLocation(GLuint location) const {
      if (location < attrib_location_to_index_map_.size()) {
        GLint index = attrib_location_to_index_map_[location];
        if (index >= 0) {
          return &attrib_infos_[index];
        }
      }
      return NULL;
    }

    const UniformInfo* GetUniformInfo(GLint index) const {
      return (static_cast<size_t>(index) < uniform_infos_.size()) ?
         &uniform_infos_[index] : NULL;
    }

    // If the original name is not found, return NULL.
    const std::string* GetAttribMappedName(
        const std::string& original_name) const;

    // Gets the fake location of a uniform by name.
    GLint GetUniformFakeLocation(const std::string& name) const;

    // Gets the UniformInfo of a uniform by location.
    const UniformInfo* GetUniformInfoByFakeLocation(
        GLint fake_location, GLint* real_location, GLint* array_index) const;

    // Gets all the program info.
    void GetProgramInfo(
        ProgramManager* manager, CommonDecoder::Bucket* bucket) const;

    // Sets the sampler values for a uniform.
    // This is safe to call for any location. If the location is not
    // a sampler uniform nothing will happen.
    // Returns false if fake_location is a sampler and any value
    // is >= num_texture_units. Returns true otherwise.
    bool SetSamplers(
        GLint num_texture_units, GLint fake_location,
        GLsizei count, const GLint* value);

    bool IsDeleted() const {
      return deleted_;
    }

    void GetProgramiv(GLenum pname, GLint* params);

    bool IsValid() const {
      return valid_;
    }

    bool AttachShader(ShaderManager* manager, ShaderManager::ShaderInfo* info);
    bool DetachShader(ShaderManager* manager, ShaderManager::ShaderInfo* info);

    bool CanLink() const;

    // Performs glLinkProgram and related activities.
    bool Link();

    // Performs glValidateProgram and related activities.
    void Validate();

    const std::string* log_info() const {
      return log_info_.get();
    }

    bool InUse() const {
      DCHECK_GE(use_count_, 0);
      return use_count_ != 0;
    }

    // Sets attribute-location binding from a glBindAttribLocation() call.
    void SetAttribLocationBinding(const std::string& attrib,
                                  GLint location) {
      bind_attrib_location_map_[attrib] = location;
    }

    // Detects if there are attribute location conflicts from
    // glBindAttribLocation() calls.
    // We only consider the declared attributes in the program.
    bool DetectAttribLocationBindingConflicts() const;

    static inline GLint GetFakeLocation(
        GLint fake_base_location, GLint element_index) {
      return fake_base_location | element_index << 16;
    }

   private:
    friend class base::RefCounted<ProgramInfo>;
    friend class ProgramManager;

    ~ProgramInfo();

    void set_log_info(const char* str) {
      log_info_.reset(str ? new std::string(str) : NULL);
    }

    void ClearLinkStatus() {
      link_status_ = false;
    }

    void IncUseCount() {
      ++use_count_;
    }

    void DecUseCount() {
      --use_count_;
      DCHECK_GE(use_count_, 0);
    }

    void MarkAsDeleted() {
      DCHECK(!deleted_);
      deleted_ =  true;
    }

    // Resets the program.
    void Reset();

    // Updates the program info after a successful link.
    void Update();

    // Updates the program log info from GL
    void UpdateLogInfo();

    // Clears all the uniforms.
    void ClearUniforms(std::vector<uint8>* zero_buffer);

    // If long attribate names are mapped during shader translation, call
    // glBindAttribLocation() again with the mapped names.
    // This is called right before the glLink() call, but after shaders are
    // translated.
    void ExecuteBindAttribLocationCalls();

    const UniformInfo* AddUniformInfo(
        GLsizei size, GLenum type, GLint location,
        const std::string& name, const std::string& original_name);

    void GetCorrectedVariableInfo(
        bool use_uniforms, const std::string& name, std::string* corrected_name,
        std::string* original_name, GLsizei* size, GLenum* type) const;

    void DetachShaders(ShaderManager* manager);

    static inline GLint GetUniformInfoIndexFromFakeLocation(
        GLint fake_location) {
      return fake_location & 0xFFFF;
    }

    static inline GLint GetArrayElementIndexFromFakeLocation(
        GLint fake_location) {
      return (fake_location >> 16) & 0xFFFF;
    }

    ProgramManager* manager_;

    int use_count_;

    GLsizei max_attrib_name_length_;

    // Attrib by index.
    AttribInfoVector attrib_infos_;

    // Attrib by location to index.
    std::vector<GLint> attrib_location_to_index_map_;

    GLsizei max_uniform_name_length_;

    // Uniform info by index.
    UniformInfoVector uniform_infos_;

    // The indices of the uniforms that are samplers.
    SamplerIndices sampler_indices_;

    // The program this ProgramInfo is tracking.
    GLuint service_id_;

    // Shaders by type of shader.
    ShaderManager::ShaderInfo::Ref attached_shaders_[kMaxAttachedShaders];

    // True if this program is marked as deleted.
    bool deleted_;

    // This is true if glLinkProgram was successful at least once.
    bool valid_;

    // This is true if glLinkProgram was successful last time it was called.
    bool link_status_;

    // True if the uniforms have been cleared.
    bool uniforms_cleared_;

    // Log info
    scoped_ptr<std::string> log_info_;

    // attribute-location binding map from glBindAttribLocation() calls.
    std::map<std::string, GLint> bind_attrib_location_map_;
  };

  ProgramManager();
  ~ProgramManager();

  // Must call before destruction.
  void Destroy(bool have_context);

  // Creates a new program info.
  ProgramInfo* CreateProgramInfo(GLuint client_id, GLuint service_id);

  // Gets a program info
  ProgramInfo* GetProgramInfo(GLuint client_id);

  // Gets a client id for a given service id.
  bool GetClientId(GLuint service_id, GLuint* client_id) const;

  // Marks a program as deleted. If it is not used the info will be deleted.
  void MarkAsDeleted(ShaderManager* shader_manager, ProgramInfo* info);

  // Marks a program as used.
  void UseProgram(ProgramInfo* info);

  // Makes a program as unused. If deleted the program info will be removed.
  void UnuseProgram(ShaderManager* shader_manager, ProgramInfo* info);

  // Clears the uniforms for this program.
  void ClearUniforms(ProgramInfo* info);

  // Returns true if prefix is invalid for gl.
  static bool IsInvalidPrefix(const char* name, size_t length);

  // Check if a ProgramInfo is owned by this ProgramManager.
  bool IsOwned(ProgramInfo* info);

  GLint SwizzleLocation(GLint unswizzled_location) const;
  GLint UnswizzleLocation(GLint swizzled_location) const;

 private:
  void StartTracking(ProgramInfo* info);
  void StopTracking(ProgramInfo* info);

  // Info for each "successfully linked" program by service side program Id.
  // TODO(gman): Choose a faster container.
  typedef std::map<GLuint, ProgramInfo::Ref> ProgramInfoMap;
  ProgramInfoMap program_infos_;

  int uniform_swizzle_;

  // Counts the number of ProgramInfo allocated with 'this' as its manager.
  // Allows to check no ProgramInfo will outlive this.
  unsigned int program_info_count_;

  bool have_context_;

  bool disable_workarounds_;

  // Used to clear uniforms.
  std::vector<uint8> zero_;

  void RemoveProgramInfoIfUnused(
      ShaderManager* shader_manager, ProgramInfo* info);

  DISALLOW_COPY_AND_ASSIGN(ProgramManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_PROGRAM_MANAGER_H_
