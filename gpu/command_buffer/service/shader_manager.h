// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHADER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHADER_MANAGER_H_

#include <map>
#include <string>
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
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
        : use_count_(0),
          service_id_(service_id),
          shader_type_(shader_type),
          valid_(false) {
    }

    void Update(const char* source) {
      source_.reset(source ? new std::string(source) : NULL);
    }

    GLuint service_id() const {
      return service_id_;
    }

    GLenum shader_type() const {
      return shader_type_;
    }

    const std::string* source() const {
      return source_.get();
    }

    void SetStatus(
        bool valid, const char* log,
        ShaderTranslatorInterface* translator);

    const VariableInfo* GetAttribInfo(const std::string& name) const;
    const VariableInfo* GetUniformInfo(const std::string& name) const;

    const std::string* log_info() const {
      return log_info_.get();
    }

    bool IsValid() const {
      return valid_;
    }

    bool IsDeleted() const {
      return service_id_ == 0;
    }

    bool InUse() const {
      DCHECK_GE(use_count_, 0);
      return use_count_ != 0;
    }

   private:
    typedef ShaderTranslator::VariableMap VariableMap;

    friend class base::RefCounted<ShaderInfo>;
    friend class ShaderManager;
    ~ShaderInfo() { }

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

    int use_count_;

    // The shader this ShaderInfo is tracking.
    GLuint service_id_;
    // Type of shader - GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
    GLenum shader_type_;

    // True if compilation succeeded.
    bool valid_;

    // The shader source as passed to glShaderSource.
    scoped_ptr<std::string> source_;

    // The shader translation log.
    scoped_ptr<std::string> log_info_;

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

  // Gets a client id for a given service id.
  bool GetClientId(GLuint service_id, GLuint* client_id) const;

  void MarkAsDeleted(ShaderInfo* info);

  // Mark a shader as used
  void UseShader(ShaderInfo* info);

  // Unmark a shader as used. If it has been deleted and is not used
  // then we free the info.
  void UnuseShader(ShaderInfo* info);

 private:
  // Info for each shader by service side shader Id.
  typedef std::map<GLuint, ShaderInfo::Ref> ShaderInfoMap;
  ShaderInfoMap shader_infos_;

  void RemoveShaderInfoIfUnused(ShaderInfo* info);

  DISALLOW_COPY_AND_ASSIGN(ShaderManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHADER_MANAGER_H_

