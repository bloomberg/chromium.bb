// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHADER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHADER_MANAGER_H_

#include <map>
#include <string>
#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shader_translator.h"

namespace gpu {
namespace gles2 {

// Tracks the Shaders.
//
// NOTE: To support shared resources an instance of this class will
// need to be shared by multiple GLES2Decoders.
class ShaderManager {
 public:
  // This is used to keep the source code for a shader. This is because in order
  // to emluate GLES2 the shaders will have to be re-written before passed to
  // the underlying OpenGL. But, when the user calls glGetShaderSource they
  // should get the source they passed in, not the re-written source.
  class ShaderInfo : public base::RefCounted<ShaderInfo> {
   public:
    typedef scoped_refptr<ShaderInfo> Ref;
    typedef ShaderTranslator::VariableInfo VariableInfo;

    explicit ShaderInfo(GLuint service_id, GLenum shader_type)
        : service_id_(service_id),
          shader_type_(shader_type),
          valid_(false) {
    }

    void Update(const std::string& source) {
      source_ = source;
    }

    GLuint service_id() const {
      return service_id_;
    }

    GLenum shader_type() const {
      return shader_type_;
    }

    const std::string& source() const {
      return source_;
    }

    void SetStatus(
        bool valid, const std::string& log,
        ShaderTranslatorInterface* translator);

    const VariableInfo* GetAttribInfo(const std::string& name) const;
    const VariableInfo* GetUniformInfo(const std::string& name) const;

    const std::string& log_info() const {
      return log_info_;
    }

    bool IsValid() const {
      return valid_;
    }

    bool IsDeleted() const {
      return service_id_ == 0;
    }

   private:
    typedef ShaderTranslator::VariableMap VariableMap;

    friend class base::RefCounted<ShaderInfo>;
    friend class ShaderManager;
    ~ShaderInfo() { }

    void MarkAsDeleted() {
      service_id_ = 0;
    }

    // The shader this ShaderInfo is tracking.
    GLuint service_id_;
    // Type of shader - GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
    GLenum shader_type_;

    // True if compilation succeeded.
    bool valid_;

    // The shader source as passed to glShaderSource.
    std::string source_;

    // The shader translation log.
    std::string log_info_;

    // The type info when the shader was last compiled.
    VariableMap attrib_map_;
    VariableMap uniform_map_;
  };

  ShaderManager();
  ~ShaderManager();

  // Must call before destruction.
  void Destroy(bool have_context);

  // Creates a shader info for the given shader ID.
  void CreateShaderInfo(GLuint client_id,
                        GLuint service_id,
                        GLenum shader_type);

  // Gets an existing shader info for the given shader ID. Returns NULL if none
  // exists.
  ShaderInfo* GetShaderInfo(GLuint client_id);

  // Deletes the shader info for the given shader.
  void RemoveShaderInfo(GLuint client_id);

  // Gets a client id for a given service id.
  bool GetClientId(GLuint service_id, GLuint* client_id) const;

 private:
  // Info for each shader by service side shader Id.
  typedef std::map<GLuint, ShaderInfo::Ref> ShaderInfoMap;
  ShaderInfoMap shader_infos_;

  DISALLOW_COPY_AND_ASSIGN(ShaderManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHADER_MANAGER_H_

