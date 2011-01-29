// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_PROGRAM_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_PROGRAM_MANAGER_H_

#include <map>
#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shader_manager.h"

namespace gpu {
namespace gles2 {

// Tracks the Programs.
//
// NOTE: To support shared resources an instance of this class will
// need to be shared by multiple GLES2Decoders.
class ProgramManager {
 public:
  // This is used to track which attributes a particular program needs
  // so we can verify at glDrawXXX time that every attribute is either disabled
  // or if enabled that it points to a valid source.
  class ProgramInfo : public base::RefCounted<ProgramInfo> {
   public:
    typedef scoped_refptr<ProgramInfo> Ref;

    static const int kMaxAttachedShaders = 2;

    struct UniformInfo {
      UniformInfo(GLsizei _size, GLenum _type, const std::string& _name);
      ~UniformInfo();

      bool IsSampler() const {
        return type == GL_SAMPLER_2D || type == GL_SAMPLER_CUBE;
      }

      GLsizei size;
      GLenum type;
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

    explicit ProgramInfo(GLuint service_id);

    GLuint service_id() const {
      return service_id_;
    }

    const SamplerIndices& sampler_indices() {
      return sampler_indices_;
    }

    // Updates the program info after a successful link.
    void Update();

    // Updates the program log info.
    void UpdateLogInfo();

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

    // Gets the location of a uniform by name.
    GLint GetUniformLocation(const std::string& name) const;

    // Gets the type of a uniform by location.
    bool GetUniformTypeByLocation(GLint location, GLenum* type) const;

    // Sets the sampler values for a uniform.
    // This is safe to call for any location. If the location is not
    // a sampler uniform nothing will happen.
    bool SetSamplers(GLint location, GLsizei count, const GLint* value);

    bool IsDeleted() const {
      return service_id_ == 0;
    }

    void GetProgramiv(GLenum pname, GLint* params);

    bool IsValid() const {
      return valid_;
    }

    void ClearLinkStatus() {
      link_status_ = false;
    }

    bool AttachShader(ShaderManager* manager, ShaderManager::ShaderInfo* info);
    void DetachShader(ShaderManager* manager, ShaderManager::ShaderInfo* info);

    bool CanLink() const;

    const std::string* log_info() const {
      return log_info_.get();
    }

    void set_log_info(const char* str) {
      log_info_.reset(str ? new std::string(str) : NULL);
    }

    bool InUse() const {
      DCHECK_GE(use_count_, 0);
      return use_count_ != 0;
    }

   private:
    friend class base::RefCounted<ProgramInfo>;
    friend class ProgramManager;

    ~ProgramInfo();

    void IncUseCount() {
      ++use_count_;
    }

    void DecUseCount() {
      --use_count_;
      DCHECK_GE(use_count_, 0);
    }

    void MarkAsDeleted() {
      DCHECK_NE(service_id_, 0u);
      service_id_ = 0;
    }

    // Resets the program.
    void Reset();

    const UniformInfo* AddUniformInfo(
        GLsizei size, GLenum type, GLint location, const std::string& name);

    void GetCorrectedVariableInfo(
        bool use_uniforms, const std::string& name, std::string* corrected_name,
        GLsizei* size, GLenum* type) const;

    void DetachShaders(ShaderManager* manager);

    int use_count_;

    GLsizei max_attrib_name_length_;

    // Attrib by index.
    AttribInfoVector attrib_infos_;

    // Attrib by location to index.
    std::vector<GLint> attrib_location_to_index_map_;

    GLsizei max_uniform_name_length_;

    // Uniform info by index.
    UniformInfoVector uniform_infos_;

    // Uniform location to index.
    std::vector<GLint> uniform_location_to_index_map_;

    // The indices of the uniforms that are samplers.
    SamplerIndices sampler_indices_;

    // The program this ProgramInfo is tracking.
    GLuint service_id_;

    // Shaders by type of shader.
    ShaderManager::ShaderInfo::Ref attached_shaders_[kMaxAttachedShaders];

    // This is true if glLinkProgram was successful at least once.
    bool valid_;

    // This is true if glLinkProgram was successful last time it was called.
    bool link_status_;

    // Log info
    scoped_ptr<std::string> log_info_;
  };

  ProgramManager();
  ~ProgramManager();

  // Must call before destruction.
  void Destroy(bool have_context);

  // Creates a new program info.
  void CreateProgramInfo(GLuint client_id, GLuint service_id);

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

  // Returns true if prefix is invalid for gl.
  static bool IsInvalidPrefix(const char* name, size_t length);

 private:
  // Info for each "successfully linked" program by service side program Id.
  // TODO(gman): Choose a faster container.
  typedef std::map<GLuint, ProgramInfo::Ref> ProgramInfoMap;
  ProgramInfoMap program_infos_;

  void RemoveProgramInfoIfUnused(
      ShaderManager* shader_manager, ProgramInfo* info);

  DISALLOW_COPY_AND_ASSIGN(ProgramManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_PROGRAM_MANAGER_H_
