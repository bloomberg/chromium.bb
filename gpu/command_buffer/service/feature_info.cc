// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include "app/gfx/gl/gl_implementation.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/GLES2/gles2_command_buffer.h"

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
  disallowed_extensions_ = DisallowedExtensions();
  AddFeatures(allowed_features);
  return true;
}

bool FeatureInfo::Initialize(const DisallowedExtensions& disallowed_extensions,
                             const char* allowed_features) {
  disallowed_extensions_ = disallowed_extensions;
  AddFeatures(allowed_features);
  return true;
}

void FeatureInfo::AddFeatures(const char* desired_features) {
  // Figure out what extensions to turn on.
  ExtensionHelper ext(
      reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)),
      desired_features);

  bool npot_ok = false;

  AddExtensionString("GL_CHROMIUM_map_sub");
  AddExtensionString("GL_CHROMIUM_copy_texture_to_parent_texture");
  AddExtensionString("GL_CHROMIUM_resource_safe");
  AddExtensionString("GL_CHROMIUM_resize");
  AddExtensionString("GL_CHROMIUM_strict_attribs");

  // Only turn this feature on if it is requested. Not by default.
  if (desired_features && ext.Desire("GL_CHROMIUM_webglsl")) {
    AddExtensionString("GL_CHROMIUM_webglsl");
    feature_flags_.chromium_webglsl = true;
  }

  // Check if we should allow GL_EXT_texture_compression_dxt1 and
  // GL_EXT_texture_compression_s3tc.
  bool enable_dxt1 = false;
  bool enable_s3tc = false;

  if (ext.HaveAndDesire("GL_EXT_texture_compression_dxt1")) {
    enable_dxt1 = true;
  }
  if (ext.HaveAndDesire("GL_EXT_texture_compression_s3tc")) {
    enable_dxt1 = true;
    enable_s3tc = true;
  }

  if (enable_dxt1) {
    AddExtensionString("GL_EXT_texture_compression_dxt1");
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGBA_S3TC_DXT1_EXT);
  }

  if (enable_s3tc) {
    AddExtensionString("GL_EXT_texture_compression_s3tc");
    validators_.compressed_texture_format.AddValue(
        GL_COMPRESSED_RGBA_S3TC_DXT3_EXT);
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
    if (ext.Have("GL_OES_rgb8_rgba8") ||
        gfx::GetGLImplementation() == gfx::kGLImplementationDesktopGL) {
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
  if (!disallowed_extensions_.multisampling &&
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
      (gfx::GetGLImplementation() == gfx::kGLImplementationDesktopGL &&
       ext.Desire("GL_OES_depth24"))) {
    AddExtensionString("GL_OES_depth24");
    validators_.render_buffer_format.AddValue(GL_DEPTH_COMPONENT24);
  }

  if (ext.HaveAndDesire("GL_OES_standard_derivatives") ||
      (gfx::GetGLImplementation() == gfx::kGLImplementationDesktopGL &&
       ext.Desire("GL_OES_standard_derivatives"))) {
    AddExtensionString("GL_OES_standard_derivatives");
    feature_flags_.oes_standard_derivatives = true;
    validators_.hint_target.AddValue(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
    validators_.g_l_state.AddValue(GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
  }

  // TODO(gman): Add support for these extensions.
  //     GL_OES_depth32
  //     GL_OES_element_index_uint

  feature_flags_.enable_texture_float_linear = enable_texture_float_linear;
  feature_flags_.enable_texture_half_float_linear =
      enable_texture_half_float_linear;
  feature_flags_.npot_ok = npot_ok;
}

void FeatureInfo::AddExtensionString(const std::string& str) {
  if (extensions_.find(str) == std::string::npos) {
    extensions_ += (extensions_.empty() ? "" : " ") + str;
  }
}

}  // namespace gles2
}  // namespace gpu



