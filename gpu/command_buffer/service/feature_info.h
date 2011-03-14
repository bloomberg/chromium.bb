// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_FEATURE_INFO_H_
#define GPU_COMMAND_BUFFER_SERVICE_FEATURE_INFO_H_

#include <string>
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_validation.h"

namespace gpu {
namespace gles2 {

// FeatureInfo records the features that are available for a particular context.
class FeatureInfo {
 public:
  struct FeatureFlags {
    FeatureFlags()
        : chromium_framebuffer_multisample(false),
          oes_standard_derivatives(false),
          npot_ok(false),
          enable_texture_float_linear(false),
          enable_texture_half_float_linear(false),
          chromium_webglsl(false) {
    }

    bool chromium_framebuffer_multisample;
    bool oes_standard_derivatives;
    bool npot_ok;
    bool enable_texture_float_linear;
    bool enable_texture_half_float_linear;
    bool chromium_webglsl;
  };

  FeatureInfo();
  ~FeatureInfo();

  // If allowed features = NULL or "*", all features are allowed. Otherwise
  // only features that match the strings in allowed_features are allowed.
  bool Initialize(const char* allowed_features);
  bool Initialize(const DisallowedExtensions& disallowed_extensions,
                  const char* allowed_features);

  // Turns on certain features if they can be turned on. NULL turns on
  // all available features.
  void AddFeatures(const char* desired_features);

  const Validators* validators() const {
    return &validators_;
  }

  const std::string& extensions() const {
    return extensions_;
  }

  const FeatureFlags& feature_flags() const {
    return feature_flags_;
  }

 private:
  void AddExtensionString(const std::string& str);

  Validators validators_;

  DisallowedExtensions disallowed_extensions_;

  // The extensions string returned by glGetString(GL_EXTENSIONS);
  std::string extensions_;

  // Flags for some features
  FeatureFlags feature_flags_;

  DISALLOW_COPY_AND_ASSIGN(FeatureInfo);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_FEATURE_INFO_H_


