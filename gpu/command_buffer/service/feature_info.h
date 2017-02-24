// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_FEATURE_INFO_H_
#define GPU_COMMAND_BUFFER_SERVICE_FEATURE_INFO_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_validation.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/gpu_export.h"

namespace base {
class CommandLine;
}

namespace gl {
struct GLVersionInfo;
}

namespace gpu {
namespace gles2 {

// FeatureInfo records the features that are available for a ContextGroup.
class GPU_EXPORT FeatureInfo : public base::RefCounted<FeatureInfo> {
 public:
  struct FeatureFlags {
    FeatureFlags();

    bool chromium_framebuffer_multisample = false;
    bool chromium_sync_query = false;
    // Use glBlitFramebuffer() and glRenderbufferStorageMultisample() with
    // GL_EXT_framebuffer_multisample-style semantics, since they are exposed
    // as core GL functions on this implementation.
    bool use_core_framebuffer_multisample = false;
    bool multisampled_render_to_texture = false;
    // Use the IMG GLenum values and functions rather than EXT.
    bool use_img_for_multisampled_render_to_texture = false;
    bool chromium_screen_space_antialiasing = false;
    bool use_chromium_screen_space_antialiasing_via_shaders = false;
    bool oes_standard_derivatives = false;
    bool oes_egl_image_external = false;
    bool nv_egl_stream_consumer_external = false;
    bool oes_depth24 = false;
    bool oes_compressed_etc1_rgb8_texture = false;
    bool packed_depth24_stencil8 = false;
    bool npot_ok = false;
    bool enable_texture_float_linear = false;
    bool enable_texture_half_float_linear = false;
    bool enable_color_buffer_float = false;
    bool enable_color_buffer_half_float = false;
    bool angle_translated_shader_source = false;
    bool angle_pack_reverse_row_order = false;
    bool arb_texture_rectangle = false;
    bool angle_instanced_arrays = false;
    bool occlusion_query = false;
    bool occlusion_query_boolean = false;
    bool use_arb_occlusion_query2_for_occlusion_query_boolean = false;
    bool use_arb_occlusion_query_for_occlusion_query_boolean = false;
    bool native_vertex_array_object = false;
    bool ext_texture_format_astc = false;
    bool ext_texture_format_atc = false;
    bool ext_texture_format_bgra8888 = false;
    bool ext_texture_format_dxt1 = false;
    bool ext_texture_format_dxt5 = false;
    bool enable_shader_name_hashing = false;
    bool enable_samplers = false;
    bool ext_draw_buffers = false;
    bool nv_draw_buffers = false;
    bool ext_frag_depth = false;
    bool ext_shader_texture_lod = false;
    bool use_async_readpixels = false;
    bool map_buffer_range = false;
    bool ext_discard_framebuffer = false;
    bool angle_depth_texture = false;
    bool is_swiftshader_for_webgl = false;
    bool angle_texture_usage = false;
    bool ext_texture_storage = false;
    bool chromium_path_rendering = false;
    bool chromium_framebuffer_mixed_samples = false;
    bool blend_equation_advanced = false;
    bool blend_equation_advanced_coherent = false;
    bool ext_texture_rg = false;
    bool chromium_image_ycbcr_420v = false;
    bool chromium_image_ycbcr_422 = false;
    bool emulate_primitive_restart_fixed_index = false;
    bool ext_render_buffer_format_bgra8888 = false;
    bool ext_multisample_compatibility = false;
    bool ext_blend_func_extended = false;
    bool ext_read_format_bgra = false;
    bool desktop_srgb_support = false;
    bool arb_es3_compatibility = false;
    bool chromium_color_buffer_float_rgb = false;
    bool chromium_color_buffer_float_rgba = false;
    bool angle_robust_client_memory = false;
    bool khr_debug = false;
    bool chromium_bind_generates_resource = false;
    bool angle_webgl_compatibility = false;
    bool ext_srgb_write_control = false;
    bool ext_srgb = false;
    bool chromium_copy_texture = false;
    bool chromium_copy_compressed_texture = false;
    bool angle_framebuffer_multisample = false;
    bool ext_disjoint_timer_query = false;
    bool angle_client_arrays = false;
  };

  FeatureInfo();

  // Constructor with workarounds taken from the current process's CommandLine
  explicit FeatureInfo(
      const GpuDriverBugWorkarounds& gpu_driver_bug_workarounds);

  // Constructor with workarounds taken from |command_line|.
  FeatureInfo(const base::CommandLine& command_line,
              const GpuDriverBugWorkarounds& gpu_driver_bug_workarounds);

  // Initializes the feature information. Needs a current GL context.
  bool Initialize(ContextType context_type,
                  const DisallowedFeatures& disallowed_features);

  // Helper that defaults to no disallowed features and a GLES2 context.
  bool InitializeForTesting();
  // Helper that defaults to no disallowed Features.
  bool InitializeForTesting(ContextType context_type);
  // Helper that defaults to a GLES2 context.
  bool InitializeForTesting(const DisallowedFeatures& disallowed_features);

  const Validators* validators() const {
    return &validators_;
  }

  ContextType context_type() const { return context_type_; }

  const std::string& extensions() const {
    return extensions_;
  }

  const FeatureFlags& feature_flags() const {
    return feature_flags_;
  }

  const GpuDriverBugWorkarounds& workarounds() const { return workarounds_; }

  const DisallowedFeatures& disallowed_features() const {
    return disallowed_features_;
  }

  const gl::GLVersionInfo& gl_version_info() const {
    DCHECK(gl_version_info_.get());
    return *(gl_version_info_.get());
  }

  bool IsES3Capable() const;
  void EnableES3Validators();

  bool disable_shader_translator() const { return disable_shader_translator_; }

  bool IsWebGLContext() const;
  bool IsWebGL1OrES2Context() const;
  bool IsWebGL2OrES3Context() const;

  void EnableCHROMIUMColorBufferFloatRGBA();
  void EnableCHROMIUMColorBufferFloatRGB();
  void EnableEXTColorBufferFloat();
  void EnableEXTColorBufferHalfFloat();
  void EnableOESTextureFloatLinear();
  void EnableOESTextureHalfFloatLinear();

  bool ext_color_buffer_float_available() const {
    return ext_color_buffer_float_available_;
  }

  bool oes_texture_float_linear_available() const {
    return oes_texture_float_linear_available_;
  }

 private:
  friend class base::RefCounted<FeatureInfo>;
  friend class BufferManagerClientSideArraysTest;
  class StringSet;

  ~FeatureInfo();

  void AddExtensionString(const char* s);
  void InitializeBasicState(const base::CommandLine* command_line);
  void InitializeFeatures();
  void InitializeFloatAndHalfFloatFeatures(const StringSet& extensions);

  Validators validators_;

  DisallowedFeatures disallowed_features_;

  ContextType context_type_;

  // The extensions string returned by glGetString(GL_EXTENSIONS);
  std::string extensions_;

  // Flags for some features
  FeatureFlags feature_flags_;

  // Flags for Workarounds.
  const GpuDriverBugWorkarounds workarounds_;

  bool ext_color_buffer_float_available_;
  bool oes_texture_float_linear_available_;
  bool oes_texture_half_float_linear_available_;

  bool disable_shader_translator_;
  std::unique_ptr<gl::GLVersionInfo> gl_version_info_;

  DISALLOW_COPY_AND_ASSIGN(FeatureInfo);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_FEATURE_INFO_H_
