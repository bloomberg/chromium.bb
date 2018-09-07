// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gl_scaler.h"

#include <utility>

#include "base/logging.h"
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/GLES2/gl2chromium.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace viz {

GLScaler::GLScaler(scoped_refptr<ContextProvider> context_provider)
    : context_provider_(std::move(context_provider)) {
  if (context_provider_) {
    DCHECK(context_provider_->ContextGL());
    context_provider_->AddObserver(this);
  }
}

GLScaler::~GLScaler() {
  OnContextLost();  // Ensures destruction in dependency order.
}

bool GLScaler::SupportsPreciseColorManagement() const {
  if (!context_provider_) {
    return false;
  }
  const gpu::Capabilities& caps = context_provider_->ContextCapabilities();
  return caps.texture_half_float_linear && caps.color_buffer_half_float_rgba;
}

int GLScaler::GetMaxDrawBuffersSupported() const {
  if (!context_provider_) {
    return 0;
  }

  if (max_draw_buffers_ < 0) {
    // Query the GL context for the multiple draw buffers extension and, if
    // present, the actual platform-supported maximum.
    GLES2Interface* const gl = context_provider_->ContextGL();
    DCHECK(gl);
    if (const auto* extensions = gl->GetString(GL_EXTENSIONS)) {
      const std::string extensions_string =
          " " + std::string(reinterpret_cast<const char*>(extensions)) + " ";
      if (extensions_string.find(" GL_EXT_draw_buffers ") !=
          std::string::npos) {
        gl->GetIntegerv(GL_MAX_DRAW_BUFFERS_EXT, &max_draw_buffers_);
      }
    }

    if (max_draw_buffers_ < 1) {
      max_draw_buffers_ = 1;
    }
  }

  return max_draw_buffers_;
}

bool GLScaler::Configure(const Parameters& new_params) {
  if (!context_provider_) {
    return false;
  }
  GLES2Interface* const gl = context_provider_->ContextGL();
  DCHECK(gl);

  params_ = new_params;

  // Ensure the client has provided valid scaling vectors.
  if (params_.scale_from.x() == 0 || params_.scale_from.y() == 0 ||
      params_.scale_to.x() == 0 || params_.scale_to.y() == 0) {
    // The caller computed invalid scale_from and/or scale_to values.
    DVLOG(1) << __func__ << ": Invalid scaling vectors: scale_from="
             << params_.scale_from.ToString()
             << ", scale_to=" << params_.scale_to.ToString();
    return false;
  }

  // Resolve the color spaces according to the rules described in the header
  // file.
  if (!params_.source_color_space.IsValid()) {
    params_.source_color_space = gfx::ColorSpace::CreateSRGB();
  }
  if (!params_.output_color_space.IsValid()) {
    params_.output_color_space = params_.source_color_space;
  }

  // Check that 16-bit half floats are supported if precise color management is
  // being requested.
  if (params_.enable_precise_color_management) {
    if (!SupportsPreciseColorManagement()) {
      DVLOG(1) << __func__
               << ": GL context does not support the half-floats "
                  "required for precise color management.";
      return false;
    }
  }

  // Check that MRT support is available if certain export formats were
  // specified in the Parameters.
  if (params_.export_format == Parameters::ExportFormat::NV61 ||
      params_.export_format ==
          Parameters::ExportFormat::DEINTERLEAVE_PAIRWISE) {
    if (GetMaxDrawBuffersSupported() < 2) {
      DVLOG(1) << __func__ << ": GL context does not support 2+ draw buffers.";
      return false;
    }
  }

  // Check that one of the two implemented output swizzles has been specified.
  for (GLenum s : params_.swizzle) {
    if (s != GL_RGBA && s != GL_BGRA_EXT) {
      NOTIMPLEMENTED();
      return false;
    }
  }

  return true;
}

bool GLScaler::ScaleToMultipleOutputs(GLuint src_texture,
                                      const gfx::Size& src_texture_size,
                                      const gfx::Vector2dF& src_offset,
                                      GLuint dest_texture_0,
                                      GLuint dest_texture_1,
                                      const gfx::Rect& output_rect) {
  NOTIMPLEMENTED();
  return false;
}

bool GLScaler::ComputeRegionOfInfluence(const gfx::Size& src_texture_size,
                                        const gfx::Vector2dF& src_offset,
                                        const gfx::Rect& output_rect,
                                        gfx::Rect* sampling_rect,
                                        gfx::Vector2dF* offset) const {
  NOTIMPLEMENTED();
  return false;
}

// static
bool GLScaler::ParametersHasSameScaleRatio(const GLScaler::Parameters& params,
                                           const gfx::Vector2d& from,
                                           const gfx::Vector2d& to) {
  // Returns true iff a_num/a_denom == b_num/b_denom.
  const auto AreRatiosEqual = [](int32_t a_num, int32_t a_denom, int32_t b_num,
                                 int32_t b_denom) -> bool {
    // The math (for each dimension):
    //   If: a_num/a_denom == b_num/b_denom
    //   Then: a_num*b_denom == b_num*a_denom
    //
    // ...and cast to int64_t to guarantee no overflow from the multiplications.
    return (static_cast<int64_t>(a_num) * b_denom) ==
           (static_cast<int64_t>(b_num) * a_denom);
  };

  return AreRatiosEqual(params.scale_from.x(), params.scale_to.x(), from.x(),
                        to.x()) &&
         AreRatiosEqual(params.scale_from.y(), params.scale_to.y(), from.y(),
                        to.y());
}

void GLScaler::OnContextLost() {
  if (context_provider_) {
    context_provider_->RemoveObserver(this);
    context_provider_ = nullptr;
  }
}

GLScaler::Parameters::Parameters() = default;
GLScaler::Parameters::~Parameters() = default;

}  // namespace viz
