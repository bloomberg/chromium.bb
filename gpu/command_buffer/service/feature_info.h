// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_FEATURE_INFO_H_
#define GPU_COMMAND_BUFFER_SERVICE_FEATURE_INFO_H_

#include <string>
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/sys_info.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_validation.h"
#include "gpu/gpu_export.h"

namespace gpu {
namespace gles2 {

// FeatureInfo records the features that are available for a ContextGroup.
class GPU_EXPORT FeatureInfo : public base::RefCounted<FeatureInfo> {
 public:
  typedef scoped_refptr<FeatureInfo> Ref;

  struct FeatureFlags {
    FeatureFlags();

    bool chromium_framebuffer_multisample;
    bool oes_standard_derivatives;
    bool oes_egl_image_external;
    bool npot_ok;
    bool enable_texture_float_linear;
    bool enable_texture_half_float_linear;
    bool chromium_stream_texture;
    bool angle_translated_shader_source;
    bool angle_pack_reverse_row_order;
    bool arb_texture_rectangle;
    bool angle_instanced_arrays;
    bool occlusion_query_boolean;
    bool use_arb_occlusion_query2_for_occlusion_query_boolean;
    bool use_arb_occlusion_query_for_occlusion_query_boolean;
    bool native_vertex_array_object;
    bool disable_workarounds;
    bool enable_shader_name_hashing;
  };

  struct Workarounds {
    Workarounds();

    bool clear_alpha_in_readpixels;
    bool clear_uniforms_before_program_use;
    bool needs_glsl_built_in_function_emulation;
    bool needs_offscreen_buffer_workaround;
    bool reverse_point_sprite_coord_origin;
    bool set_texture_filter_before_generating_mipmap;
    bool use_current_program_after_successful_link;
    bool restore_scissor_on_fbo_change;
    bool flush_on_context_switch;
    bool delete_instead_of_resize_fbo;

    // Note: 0 here means use driver limit.
    GLint max_texture_size;
    GLint max_cube_map_texture_size;
  };

  FeatureInfo();

  // If allowed features = NULL or "*", all features are allowed. Otherwise
  // only features that match the strings in allowed_features are allowed.
  bool Initialize(const char* allowed_features);
  bool Initialize(const DisallowedFeatures& disallowed_features,
                  const char* allowed_features);

  // Turns on certain features if they can be turned on.
  void AddFeatures();

  const Validators* validators() const {
    return &validators_;
  }

  const ValueValidator<GLenum>& GetTextureFormatValidator(GLenum format) {
    return texture_format_validators_[format];
  }

  const std::string& extensions() const {
    return extensions_;
  }

  const FeatureFlags& feature_flags() const {
    return feature_flags_;
  }

  const Workarounds& workarounds() const {
    return workarounds_;
  }

 private:
  friend class base::RefCounted<FeatureInfo>;

  typedef base::hash_map<GLenum, ValueValidator<GLenum> > ValidatorMap;
  ValidatorMap texture_format_validators_;

  ~FeatureInfo();

  void AddExtensionString(const std::string& str);

  Validators validators_;

  DisallowedFeatures disallowed_features_;

  // The extensions string returned by glGetString(GL_EXTENSIONS);
  std::string extensions_;

  // Flags for some features
  FeatureFlags feature_flags_;

  // Flags for Workarounds.
  Workarounds workarounds_;

  DISALLOW_COPY_AND_ASSIGN(FeatureInfo);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_FEATURE_INFO_H_
