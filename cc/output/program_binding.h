// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_PROGRAM_BINDING_H_
#define CC_OUTPUT_PROGRAM_BINDING_H_

#include <string>

#include "base/logging.h"

namespace WebKit { class WebGraphicsContext3D; }

namespace cc {

class ProgramBindingBase {
 public:
  ProgramBindingBase();
  ~ProgramBindingBase();

  void Init(WebKit::WebGraphicsContext3D* context,
            const std::string& vertex_shader,
            const std::string& fragment_shader);
  void Link(WebKit::WebGraphicsContext3D* context);
  void Cleanup(WebKit::WebGraphicsContext3D* context);

  unsigned program() const { return program_; }
  bool initialized() const { return initialized_; }

 protected:
  unsigned LoadShader(WebKit::WebGraphicsContext3D* context,
                      unsigned type,
                      const std::string& shader_source);
  unsigned CreateShaderProgram(WebKit::WebGraphicsContext3D* context,
                               unsigned vertex_shader,
                               unsigned fragment_shader);
  void CleanupShaders(WebKit::WebGraphicsContext3D* context);
  bool IsContextLost(WebKit::WebGraphicsContext3D* context);

  unsigned program_;
  unsigned vertex_shader_id_;
  unsigned fragment_shader_id_;
  bool initialized_;
};

template <class VertexShader, class FragmentShader>
class ProgramBinding : public ProgramBindingBase {
 public:
  explicit ProgramBinding(WebKit::WebGraphicsContext3D* context) {
    ProgramBindingBase::Init(context,
                             vertex_shader_.GetShaderString(),
                             fragment_shader_.GetShaderString());
  }

  void Initialize(WebKit::WebGraphicsContext3D* context,
                  bool using_bind_uniform) {
    DCHECK(context);
    DCHECK(!initialized_);

    if (IsContextLost(context))
      return;

    // Need to bind uniforms before linking
    if (!using_bind_uniform)
      Link(context);

    int base_uniform_index = 0;
    vertex_shader_.Init(
        context, program_, using_bind_uniform, &base_uniform_index);
    fragment_shader_.Init(
        context, program_, using_bind_uniform, &base_uniform_index);

    // Link after binding uniforms
    if (using_bind_uniform)
      Link(context);

    initialized_ = true;
  }

  const VertexShader& vertex_shader() const { return vertex_shader_; }
  const FragmentShader& fragment_shader() const { return fragment_shader_; }

 private:
  VertexShader vertex_shader_;
  FragmentShader fragment_shader_;
};

}  // namespace cc

#endif  // CC_OUTPUT_PROGRAM_BINDING_H_
