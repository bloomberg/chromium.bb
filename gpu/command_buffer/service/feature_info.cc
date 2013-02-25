// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/feature_info.h"

#include <set>

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/gl/gl_implementation.h"
#if defined(OS_MACOSX)
#include "ui/surface/io_surface_support_mac.h"
#endif

namespace gpu {
namespace gles2 {

namespace {

struct FormatInfo {
  GLenum format;
  const GLenum* types;
  size_t count;
};

class StringSet {
 public:
  StringSet() {}

  StringSet(const char* s) {
    Init(s);
  }

  StringSet(const std::string& str) {
    Init(str);
  }

  void Init(const char* s) {
    std::string str(s ? s : "");
    Init(str);
  }

  void Init(const std::string& str) {
    std::vector<std::string> tokens;
    Tokenize(str, " ", &tokens);
    string_set_.insert(tokens.begin(), tokens.end());
  }

  bool Contains(const char* s) {
    return string_set_.find(s) != string_set_.end();
  }

  bool Contains(const std::string& s) {
    return string_set_.find(s) != string_set_.end();
  }

 private:
  std::set<std::string> string_set_;
};

}  // anonymous namespace.

FeatureInfo::FeatureFlags::FeatureFlags()
    : chromium_framebuffer_multisample(false),
      oes_standard_derivatives(false),
      oes_egl_image_external(false),
      npot_ok(false),
      enable_texture_float_linear(false),
      enable_texture_half_float_linear(false),
      chromium_stream_texture(false),
      angle_translated_shader_source(false),
      angle_pack_reverse_row_order(false),
      arb_texture_rectangle(false),
      angle_instanced_arrays(false),
      occlusion_query_boolean(false),
      use_arb_occlusion_query2_for_occlusion_query_boolean(false),
      use_arb_occlusion_query_for_occlusion_query_boolean(false),
      native_vertex_array_object(false),
      disable_workarounds(false),
      enable_shader_name_hashing(false) {
}

FeatureInfo::Workarounds::Workarounds()
    : clear_alpha_in_readpixels(false),
      needs_glsl_built_in_function_emulation(false),
      needs_offscreen_buffer_workaround(false),
      reverse_point_sprite_coord_origin(false),
      set_texture_filter_before_generating_mipmap(false),
      use_current_program_after_successful_link(false),
      restore_scissor_on_fbo_change(false),
      flush_on_context_switch(false),
      delete_instead_of_resize_fbo(false),
      max_texture_size(0),
      max_cube_map_texture_size(0) {
}

FeatureInfo::FeatureInfo() {
  static const GLenum kAlphaTypes[] = {
      GL_UNSIGNED_BYTE,
  };
  static const GLenum kRGBTypes[] = {
      GL_UNSIGNED_BYTE,
      GL_UNSIGNED_SHORT_5_6_5,
  };
  static const GLenum kRGBATypes[] = {
      GL_UNSIGNED_BYTE,
      GL_UNSIGNED_SHORT_4_4_4_4,
      GL_UNSIGNED_SHORT_5_5_5_1,
  };
  static const GLenum kLuminanceTypes[] = {
      GL_UNSIGNED_BYTE,
  };
  static const GLenum kLuminanceAlphaTypes[] = {
      GL_UNSIGNED_BYTE,
  };
  static const FormatInfo kFormatTypes[] = {
    { GL_ALPHA, kAlphaTypes, arraysize(kAlphaTypes), },
    { GL_RGB, kRGBTypes, arraysize(kRGBTypes), },
    { GL_RGBA, kRGBATypes, arraysize(kRGBATypes), },
    { GL_LUMINANCE, kLuminanceTypes, arraysize(kLuminanceTypes), },
    { GL_LUMINANCE_ALPHA, kLuminanceAlphaTypes,
      arraysize(kLuminanceAlphaTypes), } ,
  };
  for (size_t ii = 0; ii < arraysize(kFormatTypes); ++ii) {
    const FormatInfo& info = kFormatTypes[ii];
    ValueValidator<GLenum>& validator = texture_format_validators_[info.format];
    for (size_t jj = 0; jj < info.count; ++jj) {
      validator.AddValue(info.types[jj]);
    }
  }
}

bool FeatureInfo::Initialize(const char* allowed_features) {
  disallowed_features_ = DisallowedFeatures();
  AddFeatures();
  return true;
}

bool FeatureInfo::Initialize(const DisallowedFeatures& disallowed_features,
                             const char* allowed_features) {
  disallowed_features_ = disallowed_features;
  AddFeatures();
  return true;
}

void FeatureInfo::AddFeatures() {
  // Figure out what extensions to turn on.
  StringSet extensions(
      reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));

  // NOTE: We need to check both GL_VENDOR and GL_RENDERER because for example
  // Sandy Bridge on Linux reports:
  //   GL_VENDOR: Tungsten Graphics, Inc
  //   GL_RENDERER:
  //       Mesa DRI Intel(R) Sandybridge Desktop GEM 20100330 DEVELOPMENT

  static GLenum string_ids[] = {
    GL_VENDOR,
    GL_RENDERER,
  };
  bool is_intel = false;
  bool is_nvidia = false;
  bool is_amd = false;
  bool is_mesa = false;
  bool is_qualcomm = false;
  bool is_imagination = false;
  for (size_t ii = 0; ii < arraysize(string_ids); ++ii) {
    const char* str = reinterpret_cast<const char*>(
          glGetString(string_ids[ii]));
    if (str) {
      std::string lstr(StringToLowerASCII(std::string(str)));
      StringSet string_set(lstr);
      is_intel |= string_set.Contains("intel");
      is_nvidia |= string_set.Contains("nvidia");
      is_amd |= string_set.Contains("amd") || string_set.Contains("ati");
      is_mesa |= string_set.Contains("mesa");
      is_qualcomm |= string_set.Contains("qualcomm");
      is_imagination |= string_set.Contains("imagination");
    }
  }

  feature_flags_.disable_workarounds =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuDriverBugWorkarounds);

  feature_flags_.enable_shader_name_hashing =
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableShaderNameHashing);


  bool npot_ok = false;

  AddExtensionString("GL_ANGLE_translated_shader_source");
  AddExtensionString("GL_CHROMIUM_async_pixel_transfers");
  AddExtensionString("GL_CHROMIUM_bind_uniform_location");
  AddExtensionString("GL_CHROMIUM_command_buffer_query");
  AddExtensionString("GL_CHROMIUM_command_buffer_latency_query");
  AddExtensionString("GL_CHROMIUM_copy_texture");
  AddExtensionString("GL_CHROMIUM_discard_backbuffer");
  AddExtensionString("GL_CHROMIUM_get_error_query");
  AddExtensionString("GL_CHROMIUM_lose_context");
  AddExtensionString("GL_CHROMIUM_pixel_transfer_buffer_object");
  AddExtensionString("GL_CHROMIUM_rate_limit_offscreen_context");
  AddExtensionString("GL_CHROMIUM_resize");
  AddExtensionString("GL_CHROMIUM_resource_safe");
  AddExtensionString("GL_CHROMIUM_set_visibility");
  AddExtensionString("GL_CHROMIUM_strict_attribs");
  AddExtensionString("GL_CHROMIUM_stream_texture");
  AddExtensionString("GL_CHROMIUM_texture_mailbox");
  AddExtensionString("GL_EXT_debug_marker");

  // Add extension to indicate fast-path texture uploads. This is
  // for IMG, where everything except async + non-power-of-two +
  // multiple-of-eight textures are brutally slow.
  if (is_imagination)
    AddExtensionString("GL_CHROMIUM_fast_NPOT_MO8_textures");

  feature_flags_.chromium_stream_texture = true;

  // OES_vertex_array_object is emulated if not present natively,
  // so the extension string is always exposed.
  AddExtensionString("GL_OES_vertex_array_object");

  if (!disallowed_features_.gpu_memory_manager)
    AddExtensionString("GL_CHROMIUM_gpu_memory_manager");

  if (extensions.Contains("GL_ANGLE_translated_shader_source")) {
    feature_flags_.angle_translated_shader_source = true;
  }

  // Check if we should allow GL_EXT_texture_compression_dxt1 and
  // GL_EXT_texture_compression_s3tc.
  bool enable_dxt1 = false;
  bool enable_dxt3 = false;
  bool enable_dxt5 = false;
  bool have_s3tc = extensions.Contains("GL_EXT_texture_compression_s3tc");
  bool have_dxt3 =
      have_s3tc || extensions.Contains("GL_ANGLE_texture_compression_dxt3");
  bool have_dxt5 =
      have_s3tc || extensions.Contains("GL_ANGLE_texture_compression_dxt5");

  if (extensions.Contains("GL_EXT_texture_compression_dxt1") || have_s3tc) {
    enable_dxt1 = true;
  }
  if (have_dxt3) {
    enable_dxt3 = true;
  }
  if (have_dxt5) {
    enable_dxt5 = true;
  }

  if (enable_dxt1) {
    AddExtensionString("GL_EXT_texture_compression_dxt1");
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
  }

  if (enable_dxt3) {
    // The difference between GL_EXT_texture_compression_s3tc and
    // GL_CHROMIUM_texture_compression_dxt3 is that the former
    // requires on the fly compression. The latter does not.
    AddExtensionString("GL_CHROMIUM_texture_compression_dxt3");
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
  }

  if (enable_dxt5) {
    // The difference between GL_EXT_texture_compression_s3tc and
    // GL_CHROMIUM_texture_compression_dxt5 is that the former
    // requires on the fly compression. The latter does not.
    AddExtensionString("GL_CHROMIUM_texture_compression_dxt5");
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
  }

  // Check if we should enable GL_EXT_texture_filter_anisotropic.
  if (extensions.Contains("GL_EXT_texture_filter_anisotropic")) {
    AddExtensionString("GL_EXT_texture_filter_anisotropic");
    validators_.texture_parameter.AddValue(
        GL_TEXTURE_MAX_ANISOTROPY_EXT);
    validators_.g_l_state.AddValue(
        GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT);
  }

  // Check if we should support GL_OES_packed_depth_stencil and/or
  // GL_GOOGLE_depth_texture / GL_CHROMIUM_depth_texture.
  //
  // NOTE: GL_OES_depth_texture requires support for depth cubemaps.
  // GL_ARB_depth_texture requires other features that
  // GL_OES_packed_depth_stencil does not provide.
  //
  // Therefore we made up GL_GOOGLE_depth_texture / GL_CHROMIUM_depth_texture.
  //
  // GL_GOOGLE_depth_texture is legacy. As we exposed it into NaCl we can't
  // get rid of it.
  //
  bool enable_depth_texture = false;
  if (extensions.Contains("GL_ARB_depth_texture") ||
      extensions.Contains("GL_OES_depth_texture") ||
      extensions.Contains("GL_ANGLE_depth_texture")) {
    enable_depth_texture = true;
  }

  if (enable_depth_texture) {
    AddExtensionString("GL_CHROMIUM_depth_texture");
    AddExtensionString("GL_GOOGLE_depth_texture");
    texture_format_validators_[GL_DEPTH_COMPONENT].AddValue(GL_UNSIGNED_SHORT);
    texture_format_validators_[GL_DEPTH_COMPONENT].AddValue(GL_UNSIGNED_INT);
    validators_.texture_internal_format.AddValue(GL_DEPTH_COMPONENT);
    validators_.texture_format.AddValue(GL_DEPTH_COMPONENT);
    validators_.pixel_type.AddValue(GL_UNSIGNED_SHORT);
    validators_.pixel_type.AddValue(GL_UNSIGNED_INT);
  }

  if (extensions.Contains("GL_EXT_packed_depth_stencil") ||
      extensions.Contains("GL_OES_packed_depth_stencil")) {
    AddExtensionString("GL_OES_packed_depth_stencil");
    if (enable_depth_texture) {
      texture_format_validators_[GL_DEPTH_STENCIL].AddValue(
          GL_UNSIGNED_INT_24_8);
      validators_.texture_internal_format.AddValue(GL_DEPTH_STENCIL);
      validators_.texture_format.AddValue(GL_DEPTH_STENCIL);
      validators_.pixel_type.AddValue(GL_UNSIGNED_INT_24_8);
    }
    validators_.render_buffer_format.AddValue(GL_DEPTH24_STENCIL8);
  }

  if (extensions.Contains("GL_OES_vertex_array_object") ||
      extensions.Contains("GL_ARB_vertex_array_object") ||
      extensions.Contains("GL_APPLE_vertex_array_object")) {
    feature_flags_.native_vertex_array_object = true;
  }

  if (extensions.Contains("GL_OES_element_index_uint") ||
      gfx::HasDesktopGLFeatures()) {
    AddExtensionString("GL_OES_element_index_uint");
    validators_.index_type.AddValue(GL_UNSIGNED_INT);
  }

  bool enable_texture_format_bgra8888 = false;
  bool enable_read_format_bgra = false;
  // Check if we should allow GL_EXT_texture_format_BGRA8888
  if (extensions.Contains("GL_EXT_texture_format_BGRA8888") ||
      extensions.Contains("GL_APPLE_texture_format_BGRA8888") ||
      extensions.Contains("GL_EXT_bgra")) {
    enable_texture_format_bgra8888 = true;
  }

  if (extensions.Contains("GL_EXT_bgra")) {
    enable_texture_format_bgra8888 = true;
    enable_read_format_bgra = true;
  }

  if (extensions.Contains("GL_EXT_read_format_bgra") ||
      extensions.Contains("GL_EXT_bgra")) {
    enable_read_format_bgra = true;
  }

  if (enable_texture_format_bgra8888) {
    AddExtensionString("GL_EXT_texture_format_BGRA8888");
    texture_format_validators_[GL_BGRA_EXT].AddValue(GL_UNSIGNED_BYTE);
    validators_.texture_internal_format.AddValue(GL_BGRA_EXT);
    validators_.texture_format.AddValue(GL_BGRA_EXT);
  }

  if (enable_read_format_bgra) {
    AddExtensionString("GL_EXT_read_format_bgra");
    validators_.read_pixel_format.AddValue(GL_BGRA_EXT);
  }

  if (extensions.Contains("GL_OES_rgb8_rgba8") || gfx::HasDesktopGLFeatures()) {
    AddExtensionString("GL_OES_rgb8_rgba8");
    validators_.render_buffer_format.AddValue(GL_RGB8_OES);
    validators_.render_buffer_format.AddValue(GL_RGBA8_OES);
  }

  // Check if we should allow GL_OES_texture_npot
  if (extensions.Contains("GL_ARB_texture_non_power_of_two") ||
      extensions.Contains("GL_OES_texture_npot")) {
    AddExtensionString("GL_OES_texture_npot");
    npot_ok = true;
  }

  // Check if we should allow GL_OES_texture_float, GL_OES_texture_half_float,
  // GL_OES_texture_float_linear, GL_OES_texture_half_float_linear
  bool enable_texture_float = false;
  bool enable_texture_float_linear = false;
  bool enable_texture_half_float = false;
  bool enable_texture_half_float_linear = false;

  bool have_arb_texture_float = extensions.Contains("GL_ARB_texture_float");

  if (have_arb_texture_float) {
    enable_texture_float = true;
    enable_texture_float_linear = true;
    enable_texture_half_float = true;
    enable_texture_half_float_linear = true;
  } else {
    if (extensions.Contains("GL_OES_texture_float") || have_arb_texture_float) {
      enable_texture_float = true;
      if (extensions.Contains("GL_OES_texture_float_linear") ||
          have_arb_texture_float) {
        enable_texture_float_linear = true;
      }
    }
    if (extensions.Contains("GL_OES_texture_half_float") ||
        have_arb_texture_float) {
      enable_texture_half_float = true;
      if (extensions.Contains("GL_OES_texture_half_float_linear") ||
          have_arb_texture_float) {
        enable_texture_half_float_linear = true;
      }
    }
  }

  if (enable_texture_float) {
    texture_format_validators_[GL_ALPHA].AddValue(GL_FLOAT);
    texture_format_validators_[GL_RGB].AddValue(GL_FLOAT);
    texture_format_validators_[GL_RGBA].AddValue(GL_FLOAT);
    texture_format_validators_[GL_LUMINANCE].AddValue(GL_FLOAT);
    texture_format_validators_[GL_LUMINANCE_ALPHA].AddValue(GL_FLOAT);
    validators_.pixel_type.AddValue(GL_FLOAT);
    validators_.read_pixel_type.AddValue(GL_FLOAT);
    AddExtensionString("GL_OES_texture_float");
    if (enable_texture_float_linear) {
      AddExtensionString("GL_OES_texture_float_linear");
    }
  }

  if (enable_texture_half_float) {
    texture_format_validators_[GL_ALPHA].AddValue(GL_HALF_FLOAT_OES);
    texture_format_validators_[GL_RGB].AddValue(GL_HALF_FLOAT_OES);
    texture_format_validators_[GL_RGBA].AddValue(GL_HALF_FLOAT_OES);
    texture_format_validators_[GL_LUMINANCE].AddValue(GL_HALF_FLOAT_OES);
    texture_format_validators_[GL_LUMINANCE_ALPHA].AddValue(GL_HALF_FLOAT_OES);
    validators_.pixel_type.AddValue(GL_HALF_FLOAT_OES);
    validators_.read_pixel_type.AddValue(GL_HALF_FLOAT_OES);
    AddExtensionString("GL_OES_texture_half_float");
    if (enable_texture_half_float_linear) {
      AddExtensionString("GL_OES_texture_half_float_linear");
    }
  }

  // Check for multisample support
  bool ext_has_multisample =
      extensions.Contains("GL_EXT_framebuffer_multisample");
  if (!is_qualcomm || feature_flags_.disable_workarounds) {
    // Some Android Qualcomm drivers falsely report this ANGLE extension string.
    // See http://crbug.com/165736
    ext_has_multisample |=
       extensions.Contains("GL_ANGLE_framebuffer_multisample");
  }
  if (!disallowed_features_.multisampling && ext_has_multisample) {
    feature_flags_.chromium_framebuffer_multisample = true;
    validators_.frame_buffer_target.AddValue(GL_READ_FRAMEBUFFER_EXT);
    validators_.frame_buffer_target.AddValue(GL_DRAW_FRAMEBUFFER_EXT);
    validators_.g_l_state.AddValue(GL_READ_FRAMEBUFFER_BINDING_EXT);
    validators_.g_l_state.AddValue(GL_MAX_SAMPLES_EXT);
    validators_.render_buffer_parameter.AddValue(GL_RENDERBUFFER_SAMPLES_EXT);
    AddExtensionString("GL_CHROMIUM_framebuffer_multisample");
  }

  if (extensions.Contains("GL_OES_depth24") || gfx::HasDesktopGLFeatures()) {
    AddExtensionString("GL_OES_depth24");
    validators_.render_buffer_format.AddValue(GL_DEPTH_COMPONENT24);
  }

  if (extensions.Contains("GL_OES_standard_derivatives") ||
      gfx::HasDesktopGLFeatures()) {
    AddExtensionString("GL_OES_standard_derivatives");
    feature_flags_.oes_standard_derivatives = true;
    validators_.hint_target.AddValue(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
    validators_.g_l_state.AddValue(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
  }

  if (extensions.Contains("GL_OES_EGL_image_external")) {
    AddExtensionString("GL_OES_EGL_image_external");
    feature_flags_.oes_egl_image_external = true;
    validators_.texture_bind_target.AddValue(GL_TEXTURE_EXTERNAL_OES);
    validators_.get_tex_param_target.AddValue(GL_TEXTURE_EXTERNAL_OES);
    validators_.texture_parameter.AddValue(GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES);
    validators_.g_l_state.AddValue(GL_TEXTURE_BINDING_EXTERNAL_OES);
  }

  if (extensions.Contains("GL_OES_compressed_ETC1_RGB8_texture")) {
    AddExtensionString("GL_OES_compressed_ETC1_RGB8_texture");
    validators_.compressed_texture_format.AddValue(GL_ETC1_RGB8_OES);
  }

  // Ideally we would only expose this extension on Mac OS X, to
  // support GL_CHROMIUM_iosurface and the compositor. We don't want
  // applications to start using it; they should use ordinary non-
  // power-of-two textures. However, for unit testing purposes we
  // expose it on all supported platforms.
  if (extensions.Contains("GL_ARB_texture_rectangle")) {
    AddExtensionString("GL_ARB_texture_rectangle");
    feature_flags_.arb_texture_rectangle = true;
    validators_.texture_bind_target.AddValue(GL_TEXTURE_RECTANGLE_ARB);
    // For the moment we don't add this enum to the texture_target
    // validator. This implies that the only way to get image data into a
    // rectangular texture is via glTexImageIOSurface2DCHROMIUM, which is
    // just fine since again we don't want applications depending on this
    // extension.
    validators_.get_tex_param_target.AddValue(GL_TEXTURE_RECTANGLE_ARB);
    validators_.g_l_state.AddValue(GL_TEXTURE_BINDING_RECTANGLE_ARB);
  }

#if defined(OS_MACOSX)
  if (IOSurfaceSupport::Initialize()) {
    AddExtensionString("GL_CHROMIUM_iosurface");
  }
#endif

  // TODO(gman): Add support for these extensions.
  //     GL_OES_depth32

  feature_flags_.enable_texture_float_linear |= enable_texture_float_linear;
  feature_flags_.enable_texture_half_float_linear |=
      enable_texture_half_float_linear;
  feature_flags_.npot_ok |= npot_ok;

  if (extensions.Contains("GL_ANGLE_pack_reverse_row_order")) {
    AddExtensionString("GL_ANGLE_pack_reverse_row_order");
    feature_flags_.angle_pack_reverse_row_order = true;
    validators_.pixel_store.AddValue(GL_PACK_REVERSE_ROW_ORDER_ANGLE);
    validators_.g_l_state.AddValue(GL_PACK_REVERSE_ROW_ORDER_ANGLE);
  }

  if (extensions.Contains("GL_ANGLE_texture_usage")) {
    AddExtensionString("GL_ANGLE_texture_usage");
    validators_.texture_parameter.AddValue(GL_TEXTURE_USAGE_ANGLE);
  }

  if (extensions.Contains("GL_EXT_texture_storage")) {
    AddExtensionString("GL_EXT_texture_storage");
    validators_.texture_parameter.AddValue(GL_TEXTURE_IMMUTABLE_FORMAT_EXT);
    if (enable_texture_format_bgra8888)
        validators_.texture_internal_format_storage.AddValue(GL_BGRA8_EXT);
    if (enable_texture_float) {
        validators_.texture_internal_format_storage.AddValue(GL_RGBA32F_EXT);
        validators_.texture_internal_format_storage.AddValue(GL_RGB32F_EXT);
        validators_.texture_internal_format_storage.AddValue(GL_ALPHA32F_EXT);
        validators_.texture_internal_format_storage.AddValue(
            GL_LUMINANCE32F_EXT);
        validators_.texture_internal_format_storage.AddValue(
            GL_LUMINANCE_ALPHA32F_EXT);
    }
    if (enable_texture_half_float) {
        validators_.texture_internal_format_storage.AddValue(GL_RGBA16F_EXT);
        validators_.texture_internal_format_storage.AddValue(GL_RGB16F_EXT);
        validators_.texture_internal_format_storage.AddValue(GL_ALPHA16F_EXT);
        validators_.texture_internal_format_storage.AddValue(
            GL_LUMINANCE16F_EXT);
        validators_.texture_internal_format_storage.AddValue(
            GL_LUMINANCE_ALPHA16F_EXT);
    }
  }

  bool have_ext_occlusion_query_boolean =
      extensions.Contains("GL_EXT_occlusion_query_boolean");
  bool have_arb_occlusion_query2 =
      extensions.Contains("GL_ARB_occlusion_query2");
  bool have_arb_occlusion_query =
      extensions.Contains("GL_ARB_occlusion_query");
  bool ext_occlusion_query_disallowed = false;

#if defined(OS_LINUX)
  if (!feature_flags_.disable_workarounds) {
    // Intel drivers on Linux appear to be buggy.
    ext_occlusion_query_disallowed = is_intel;
  }
#endif

  if (!ext_occlusion_query_disallowed &&
      (have_ext_occlusion_query_boolean ||
       have_arb_occlusion_query2 ||
       have_arb_occlusion_query)) {
    AddExtensionString("GL_EXT_occlusion_query_boolean");
    feature_flags_.occlusion_query_boolean = true;
    feature_flags_.use_arb_occlusion_query2_for_occlusion_query_boolean =
        !have_ext_occlusion_query_boolean && have_arb_occlusion_query2;
    feature_flags_.use_arb_occlusion_query_for_occlusion_query_boolean =
        !have_ext_occlusion_query_boolean && have_arb_occlusion_query &&
        !have_arb_occlusion_query2;
  }

  if (extensions.Contains("GL_ANGLE_instanced_arrays") ||
      (extensions.Contains("GL_ARB_instanced_arrays") &&
       extensions.Contains("GL_ARB_draw_instanced"))) {
    AddExtensionString("GL_ANGLE_instanced_arrays");
    feature_flags_.angle_instanced_arrays = true;
    validators_.vertex_attribute.AddValue(GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE);
  }

  if (!disallowed_features_.swap_buffer_complete_callback)
    AddExtensionString("GL_CHROMIUM_swapbuffers_complete_callback");

  if (!feature_flags_.disable_workarounds) {
    workarounds_.set_texture_filter_before_generating_mipmap = true;
    workarounds_.clear_alpha_in_readpixels = true;

    if (is_nvidia) {
      workarounds_.use_current_program_after_successful_link = true;
    }

    if (is_qualcomm) {
      workarounds_.restore_scissor_on_fbo_change = true;
      workarounds_.flush_on_context_switch = true;
      // This is only needed on the ICS driver.
      workarounds_.delete_instead_of_resize_fbo = true;
    }

#if defined(OS_MACOSX)
    workarounds_.needs_offscreen_buffer_workaround = is_nvidia;
    workarounds_.needs_glsl_built_in_function_emulation = is_amd;

    if ((is_amd || is_intel) &&
        gfx::GetGLImplementation() == gfx::kGLImplementationDesktopGL) {
      workarounds_.reverse_point_sprite_coord_origin = true;
    }

    // Limit Intel on Mac to 4096 max tex size and 1024 max cube map tex size.
    // Limit AMD on Mac to 4096 max tex size and max cube map tex size.
    // TODO(gman): Update this code to check for a specific version of
    // the drivers above which we no longer need this fix.
    if (is_intel) {
      workarounds_.max_texture_size = 4096;
      workarounds_.max_cube_map_texture_size = 1024;
      // Cubemaps > 512 in size were broken before 10.7.3.
      int32 major = 0;
      int32 minor = 0;
      int32 bugfix = 0;
      base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
      if (major < 10 ||
          (major == 10 && ((minor == 7 && bugfix < 3) || (minor < 7))))
        workarounds_.max_cube_map_texture_size = 512;
    }

    if (is_amd) {
      workarounds_.max_texture_size = 4096;
      workarounds_.max_cube_map_texture_size = 4096;
    }
#endif
  }
}

void FeatureInfo::AddExtensionString(const std::string& str) {
  if (extensions_.find(str) == std::string::npos) {
    extensions_ += (extensions_.empty() ? "" : " ") + str;
  }
}

FeatureInfo::~FeatureInfo() {
}

}  // namespace gles2
}  // namespace gpu
