// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/feature_info.h"

#include <set>

#include "base/string_number_conversions.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#if defined(OS_MACOSX)
#include "ui/surface/io_surface_support_mac.h"
#endif

namespace gpu {
namespace gles2 {

FeatureInfo::FeatureInfo() {
}

FeatureInfo::~FeatureInfo() {
}

// Helps query for extensions.
class ExtensionHelper {
 public:
  ExtensionHelper(const char* extensions, const char* desired_features)
      : desire_all_features_(false) {
    // Check for "*"
    if (desired_features &&
        desired_features[0] == '*' &&
        desired_features[1] == '\0') {
      desired_features = NULL;
    }

    InitStringSet(extensions, &have_extensions_);
    InitStringSet(desired_features, &desired_extensions_);

    if (!desired_features) {
       desire_all_features_ = true;
     }
  }

  // Returns true if extension exists.
  bool Have(const char* extension) {
    return have_extensions_.find(extension) != have_extensions_.end();
  }

  // Returns true of an extension is desired. It may not exist.
  bool Desire(const char* extension) {
    return desire_all_features_ ||
           desired_extensions_.find(extension) != desired_extensions_.end();
  }

  // Returns true if an extension exists and is desired.
  bool HaveAndDesire(const char* extension) {
    return Have(extension) && Desire(extension);
  }

 private:
  void InitStringSet(const char* s, std::set<std::string>* string_set) {
    std::string str(s ? s : "");
    std::string::size_type lastPos = 0;
    while (true) {
      std::string::size_type pos = str.find_first_of(" ", lastPos);
      if (pos != std::string::npos) {
        if (pos - lastPos) {
          string_set->insert(str.substr(lastPos, pos - lastPos));
        }
        lastPos = pos + 1;
      } else {
        string_set->insert(str.substr(lastPos));
        break;
      }
    }
  }

  bool desire_all_features_;

  // Extensions that exist.
  std::set<std::string> have_extensions_;

  // Extensions that are desired but may not exist.
  std::set<std::string> desired_extensions_;
};

bool FeatureInfo::Initialize(const char* allowed_features) {
  disallowed_features_ = DisallowedFeatures();
  AddFeatures(allowed_features);
  return true;
}

bool FeatureInfo::Initialize(const DisallowedFeatures& disallowed_features,
                             const char* allowed_features) {
  disallowed_features_ = disallowed_features;
  AddFeatures(allowed_features);
  return true;
}

void FeatureInfo::AddFeatures(const char* desired_features) {
  // Figure out what extensions to turn on.
  ExtensionHelper ext(
      // Some unittests execute without a context made current
      // so fall back to glGetString
      gfx::GLContext::GetCurrent() ?
          gfx::GLContext::GetCurrent()->GetExtensions().c_str() :
          reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)),
      desired_features);

  bool npot_ok = false;

  AddExtensionString("GL_CHROMIUM_resource_safe");
  AddExtensionString("GL_CHROMIUM_resize");
  AddExtensionString("GL_CHROMIUM_strict_attribs");
  AddExtensionString("GL_CHROMIUM_rate_limit_offscreen_context");
  AddExtensionString("GL_CHROMIUM_set_visibility");
  AddExtensionString("GL_CHROMIUM_gpu_memory_manager");
  AddExtensionString("GL_CHROMIUM_discard_framebuffer");
  AddExtensionString("GL_CHROMIUM_command_buffer_query");
  AddExtensionString("GL_CHROMIUM_copy_texture");
  AddExtensionString("GL_CHROMIUM_texture_mailbox");
  AddExtensionString("GL_ANGLE_translated_shader_source");

  if (ext.Have("GL_ANGLE_translated_shader_source")) {
    feature_flags_.angle_translated_shader_source = true;
  }

  // Only turn this feature on if it is requested. Not by default.
  if (desired_features && ext.Desire("GL_CHROMIUM_webglsl")) {
    AddExtensionString("GL_CHROMIUM_webglsl");
    feature_flags_.chromium_webglsl = true;
  }

  // Check if we should allow GL_EXT_texture_compression_dxt1 and
  // GL_EXT_texture_compression_s3tc.
  bool enable_dxt1 = false;
  bool enable_dxt3 = false;
  bool enable_dxt5 = false;
  bool have_s3tc = ext.Have("GL_EXT_texture_compression_s3tc");
  bool have_dxt3 = have_s3tc || ext.Have("GL_ANGLE_texture_compression_dxt3");
  bool have_dxt5 = have_s3tc || ext.Have("GL_ANGLE_texture_compression_dxt5");

  if (ext.Desire("GL_EXT_texture_compression_dxt1") &&
      (ext.Have("GL_EXT_texture_compression_dxt1") || have_s3tc)) {
    enable_dxt1 = true;
  }
  if (have_dxt3 && ext.Desire("GL_CHROMIUM_texture_compression_dxt3")) {
    enable_dxt3 = true;
  }
  if (have_dxt5 && ext.Desire("GL_CHROMIUM_texture_compression_dxt5")) {
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
  if (ext.HaveAndDesire("GL_EXT_texture_filter_anisotropic")) {
    AddExtensionString("GL_EXT_texture_filter_anisotropic");
    validators_.texture_parameter.AddValue(
        GL_TEXTURE_MAX_ANISOTROPY_EXT);
    validators_.g_l_state.AddValue(
        GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT);
  }

  // Check if we should support GL_OES_packed_depth_stencil and/or
  // GL_GOOGLE_depth_texture.
  // NOTE: GL_OES_depth_texture requires support for depth
  // cubemaps. GL_ARB_depth_texture requires other features that
  // GL_OES_packed_depth_stencil does not provide. Therefore we made up
  // GL_GOOGLE_depth_texture.
  bool enable_depth_texture = false;
  if (ext.Desire("GL_GOOGLE_depth_texture") &&
      (ext.Have("GL_ARB_depth_texture") ||
       ext.Have("GL_OES_depth_texture"))) {
    enable_depth_texture = true;
    AddExtensionString("GL_GOOGLE_depth_texture");
    validators_.texture_internal_format.AddValue(GL_DEPTH_COMPONENT);
    validators_.texture_format.AddValue(GL_DEPTH_COMPONENT);
    validators_.pixel_type.AddValue(GL_UNSIGNED_SHORT);
    validators_.pixel_type.AddValue(GL_UNSIGNED_INT);
  }
  // TODO(gman): Add depth types fo ElementsPerGroup and BytesPerElement

  if (ext.Desire("GL_OES_packed_depth_stencil") &&
      (ext.Have("GL_EXT_packed_depth_stencil") ||
       ext.Have("GL_OES_packed_depth_stencil"))) {
    AddExtensionString("GL_OES_packed_depth_stencil");
    if (enable_depth_texture) {
      validators_.texture_internal_format.AddValue(GL_DEPTH_STENCIL);
      validators_.texture_format.AddValue(GL_DEPTH_STENCIL);
      validators_.pixel_type.AddValue(GL_UNSIGNED_INT_24_8);
    }
    validators_.render_buffer_format.AddValue(GL_DEPTH24_STENCIL8);
  }

  bool enable_texture_format_bgra8888 = false;
  bool enable_read_format_bgra = false;
  // Check if we should allow GL_EXT_texture_format_BGRA8888
  if (ext.Desire("GL_EXT_texture_format_BGRA8888") &&
      (ext.Have("GL_EXT_texture_format_BGRA8888") ||
       ext.Have("GL_APPLE_texture_format_BGRA8888") ||
       ext.Have("GL_EXT_bgra"))) {
    enable_texture_format_bgra8888 = true;
  }

  if (ext.HaveAndDesire("GL_EXT_bgra")) {
    enable_texture_format_bgra8888 = true;
    enable_read_format_bgra = true;
  }

  if (ext.Desire("GL_EXT_read_format_bgra") &&
      (ext.Have("GL_EXT_read_format_bgra") ||
       ext.Have("GL_EXT_bgra"))) {
    enable_read_format_bgra = true;
  }

  if (enable_texture_format_bgra8888) {
    AddExtensionString("GL_EXT_texture_format_BGRA8888");
    validators_.texture_internal_format.AddValue(GL_BGRA_EXT);
    validators_.texture_format.AddValue(GL_BGRA_EXT);
  }

  if (enable_read_format_bgra) {
    AddExtensionString("GL_EXT_read_format_bgra");
    validators_.read_pixel_format.AddValue(GL_BGRA_EXT);
  }

  if (ext.Desire("GL_OES_rgb8_rgba8")) {
    if (ext.Have("GL_OES_rgb8_rgba8") || gfx::HasDesktopGLFeatures()) {
      AddExtensionString("GL_OES_rgb8_rgba8");
      validators_.render_buffer_format.AddValue(GL_RGB8_OES);
      validators_.render_buffer_format.AddValue(GL_RGBA8_OES);
    }
  }

  // Check if we should allow GL_OES_texture_npot
  if (ext.Desire("GL_OES_texture_npot") &&
      (ext.Have("GL_ARB_texture_non_power_of_two") ||
       ext.Have("GL_OES_texture_npot"))) {
    AddExtensionString("GL_OES_texture_npot");
    npot_ok = true;
  }

  // Check if we should allow GL_OES_texture_float, GL_OES_texture_half_float,
  // GL_OES_texture_float_linear, GL_OES_texture_half_float_linear
  bool enable_texture_float = false;
  bool enable_texture_float_linear = false;
  bool enable_texture_half_float = false;
  bool enable_texture_half_float_linear = false;

  bool have_arb_texture_float = ext.Have("GL_ARB_texture_float");

  if (have_arb_texture_float && ext.Desire("GL_ARB_texture_float")) {
    enable_texture_float = true;
    enable_texture_float_linear = true;
    enable_texture_half_float = true;
    enable_texture_half_float_linear = true;
  } else {
    if (ext.HaveAndDesire("GL_OES_texture_float") ||
        (have_arb_texture_float &&
         ext.Desire("GL_OES_texture_float"))) {
      enable_texture_float = true;
      if (ext.HaveAndDesire("GL_OES_texture_float_linear") ||
          (have_arb_texture_float &&
           ext.Desire("GL_OES_texture_float_linear"))) {
        enable_texture_float_linear = true;
      }
    }
    if (ext.HaveAndDesire("GL_OES_texture_half_float") ||
        (have_arb_texture_float &&
         ext.Desire("GL_OES_texture_half_float"))) {
      enable_texture_half_float = true;
      if (ext.HaveAndDesire("GL_OES_texture_half_float_linear") ||
          (have_arb_texture_float &&
           ext.Desire("GL_OES_texture_half_float_linear"))) {
        enable_texture_half_float_linear = true;
      }
    }
  }

  if (enable_texture_float) {
    validators_.pixel_type.AddValue(GL_FLOAT);
    AddExtensionString("GL_OES_texture_float");
    if (enable_texture_float_linear) {
      AddExtensionString("GL_OES_texture_float_linear");
    }
  }

  if (enable_texture_half_float) {
    validators_.pixel_type.AddValue(GL_HALF_FLOAT_OES);
    AddExtensionString("GL_OES_texture_half_float");
    if (enable_texture_half_float_linear) {
      AddExtensionString("GL_OES_texture_half_float_linear");
    }
  }

  // Check for multisample support
  if (!disallowed_features_.multisampling &&
      ext.Desire("GL_CHROMIUM_framebuffer_multisample") &&
      (ext.Have("GL_EXT_framebuffer_multisample") ||
       ext.Have("GL_ANGLE_framebuffer_multisample"))) {
    feature_flags_.chromium_framebuffer_multisample = true;
    validators_.frame_buffer_target.AddValue(GL_READ_FRAMEBUFFER_EXT);
    validators_.frame_buffer_target.AddValue(GL_DRAW_FRAMEBUFFER_EXT);
    validators_.g_l_state.AddValue(GL_READ_FRAMEBUFFER_BINDING_EXT);
    validators_.g_l_state.AddValue(GL_MAX_SAMPLES_EXT);
    validators_.render_buffer_parameter.AddValue(GL_RENDERBUFFER_SAMPLES_EXT);
    AddExtensionString("GL_CHROMIUM_framebuffer_multisample");
  }

  if (ext.HaveAndDesire("GL_OES_depth24") ||
      (gfx::HasDesktopGLFeatures() && ext.Desire("GL_OES_depth24"))) {
    AddExtensionString("GL_OES_depth24");
    validators_.render_buffer_format.AddValue(GL_DEPTH_COMPONENT24);
  }

  if (ext.HaveAndDesire("GL_OES_standard_derivatives") ||
      (gfx::HasDesktopGLFeatures() &&
       ext.Desire("GL_OES_standard_derivatives"))) {
    AddExtensionString("GL_OES_standard_derivatives");
    feature_flags_.oes_standard_derivatives = true;
    validators_.hint_target.AddValue(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
    validators_.g_l_state.AddValue(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
  }

  if (ext.HaveAndDesire("GL_OES_EGL_image_external")) {
    AddExtensionString("GL_OES_EGL_image_external");
    feature_flags_.oes_egl_image_external = true;
    validators_.texture_bind_target.AddValue(GL_TEXTURE_EXTERNAL_OES);
    validators_.get_tex_param_target.AddValue(GL_TEXTURE_EXTERNAL_OES);
    validators_.texture_parameter.AddValue(GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES);
    validators_.g_l_state.AddValue(GL_TEXTURE_BINDING_EXTERNAL_OES);
  }

  if (ext.Desire("GL_CHROMIUM_stream_texture")) {
    AddExtensionString("GL_CHROMIUM_stream_texture");
    feature_flags_.chromium_stream_texture = true;
  }

  // Ideally we would only expose this extension on Mac OS X, to
  // support GL_CHROMIUM_iosurface and the compositor. We don't want
  // applications to start using it; they should use ordinary non-
  // power-of-two textures. However, for unit testing purposes we
  // expose it on all supported platforms.
  if (ext.HaveAndDesire("GL_ARB_texture_rectangle")) {
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
  //     GL_OES_element_index_uint

  feature_flags_.enable_texture_float_linear |= enable_texture_float_linear;
  feature_flags_.enable_texture_half_float_linear |=
      enable_texture_half_float_linear;
  feature_flags_.npot_ok |= npot_ok;

  if (ext.HaveAndDesire("GL_CHROMIUM_post_sub_buffer")) {
    AddExtensionString("GL_CHROMIUM_post_sub_buffer");
  }

  if (ext.HaveAndDesire("GL_CHROMIUM_front_buffer_cached")) {
    AddExtensionString("GL_CHROMIUM_front_buffer_cached");
  }

  if (ext.Desire("GL_ANGLE_pack_reverse_row_order") &&
      ext.Have("GL_ANGLE_pack_reverse_row_order")) {
    AddExtensionString("GL_ANGLE_pack_reverse_row_order");
    feature_flags_.angle_pack_reverse_row_order = true;
    validators_.pixel_store.AddValue(GL_PACK_REVERSE_ROW_ORDER_ANGLE);
    validators_.g_l_state.AddValue(GL_PACK_REVERSE_ROW_ORDER_ANGLE);
  }

  if (ext.HaveAndDesire("GL_ANGLE_texture_usage")) {
    AddExtensionString("GL_ANGLE_texture_usage");
    validators_.texture_parameter.AddValue(GL_TEXTURE_USAGE_ANGLE);
  }

  if (ext.HaveAndDesire("GL_EXT_texture_storage")) {
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
      ext.Have("GL_EXT_occlusion_query_boolean");
  bool have_arb_occlusion_query2 = ext.Have("GL_ARB_occlusion_query2");
  bool have_arb_occlusion_query = ext.Have("GL_ARB_occlusion_query");
  if (ext.Desire("GL_EXT_occlusion_query_boolean") &&
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

  if (ext.Desire("GL_ANGLE_instanced_arrays") &&
      (ext.Have("GL_ANGLE_instanced_arrays") ||
       (ext.Have("GL_ARB_instanced_arrays") &&
        ext.Have("GL_ARB_draw_instanced")))) {
    AddExtensionString("GL_ANGLE_instanced_arrays");
    feature_flags_.angle_instanced_arrays = true;
    validators_.vertex_attribute.AddValue(GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE);
  }

  if (!disallowed_features_.swap_buffer_complete_callback)
    AddExtensionString("GL_CHROMIUM_swapbuffers_complete_callback");
}

void FeatureInfo::AddExtensionString(const std::string& str) {
  if (extensions_.find(str) == std::string::npos) {
    extensions_ += (extensions_.empty() ? "" : " ") + str;
  }
}

}  // namespace gles2
}  // namespace gpu
