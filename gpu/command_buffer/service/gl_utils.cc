// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gl_utils.h"

#include <unordered_set>

#include "base/metrics/histogram.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/logger.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {
namespace gles2 {

namespace {
const char* GetDebugSourceString(GLenum source) {
  switch (source) {
    case GL_DEBUG_SOURCE_API:
      return "OpenGL";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
      return "Window System";
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
      return "Shader Compiler";
    case GL_DEBUG_SOURCE_THIRD_PARTY:
      return "Third Party";
    case GL_DEBUG_SOURCE_APPLICATION:
      return "Application";
    case GL_DEBUG_SOURCE_OTHER:
      return "Other";
    default:
      return "UNKNOWN";
  }
}

const char* GetDebugTypeString(GLenum type) {
  switch (type) {
    case GL_DEBUG_TYPE_ERROR:
      return "Error";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
      return "Deprecated behavior";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
      return "Undefined behavior";
    case GL_DEBUG_TYPE_PORTABILITY:
      return "Portability";
    case GL_DEBUG_TYPE_PERFORMANCE:
      return "Performance";
    case GL_DEBUG_TYPE_OTHER:
      return "Other";
    case GL_DEBUG_TYPE_MARKER:
      return "Marker";
    default:
      return "UNKNOWN";
  }
}

const char* GetDebugSeverityString(GLenum severity) {
  switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
      return "High";
    case GL_DEBUG_SEVERITY_MEDIUM:
      return "Medium";
    case GL_DEBUG_SEVERITY_LOW:
      return "Low";
    case GL_DEBUG_SEVERITY_NOTIFICATION:
      return "Notification";
    default:
      return "UNKNOWN";
  }
}
}  // namespace

std::vector<int> GetAllGLErrors() {
  int gl_errors[] = {
      GL_NO_ERROR,
      GL_INVALID_ENUM,
      GL_INVALID_VALUE,
      GL_INVALID_OPERATION,
      GL_INVALID_FRAMEBUFFER_OPERATION,
      GL_OUT_OF_MEMORY,
  };
  return base::CustomHistogram::ArrayToCustomRanges(gl_errors,
                                                    arraysize(gl_errors));
}

bool PrecisionMeetsSpecForHighpFloat(GLint rangeMin,
                                     GLint rangeMax,
                                     GLint precision) {
  return (rangeMin >= 62) && (rangeMax >= 62) && (precision >= 16);
}

void QueryShaderPrecisionFormat(const gl::GLVersionInfo& gl_version_info,
                                GLenum shader_type,
                                GLenum precision_type,
                                GLint* range,
                                GLint* precision) {
  switch (precision_type) {
    case GL_LOW_INT:
    case GL_MEDIUM_INT:
    case GL_HIGH_INT:
      // These values are for a 32-bit twos-complement integer format.
      range[0] = 31;
      range[1] = 30;
      *precision = 0;
      break;
    case GL_LOW_FLOAT:
    case GL_MEDIUM_FLOAT:
    case GL_HIGH_FLOAT:
      // These values are for an IEEE single-precision floating-point format.
      range[0] = 127;
      range[1] = 127;
      *precision = 23;
      break;
    default:
      NOTREACHED();
      break;
  }

  if (gl_version_info.is_es) {
    // This function is sometimes defined even though it's really just
    // a stub, so we need to set range and precision as if it weren't
    // defined before calling it.
    // On Mac OS with some GPUs, calling this generates a
    // GL_INVALID_OPERATION error. Avoid calling it on non-GLES2
    // platforms.
    glGetShaderPrecisionFormat(shader_type, precision_type, range, precision);

    // TODO(brianderson): Make the following official workarounds.

    // Some drivers have bugs where they report the ranges as a negative number.
    // Taking the absolute value here shouldn't hurt because negative numbers
    // aren't expected anyway.
    range[0] = abs(range[0]);
    range[1] = abs(range[1]);

    // If the driver reports a precision for highp float that isn't actually
    // highp, don't pretend like it's supported because shader compilation will
    // fail anyway.
    if (precision_type == GL_HIGH_FLOAT &&
        !PrecisionMeetsSpecForHighpFloat(range[0], range[1], *precision)) {
      range[0] = 0;
      range[1] = 0;
      *precision = 0;
    }
  }
}

void PopulateNumericCapabilities(Capabilities* caps,
                                 const FeatureInfo* feature_info) {
  DCHECK(caps != nullptr);

  const gl::GLVersionInfo& version_info = feature_info->gl_version_info();
  caps->VisitPrecisions([&version_info](
                            GLenum shader, GLenum type,
                            Capabilities::ShaderPrecision* shader_precision) {
    GLint range[2] = {0, 0};
    GLint precision = 0;
    QueryShaderPrecisionFormat(version_info, shader, type, range, &precision);
    shader_precision->min_range = range[0];
    shader_precision->max_range = range[1];
    shader_precision->precision = precision;
  });

  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                &caps->max_combined_texture_image_units);
  glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &caps->max_cube_map_texture_size);
  glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS,
                &caps->max_fragment_uniform_vectors);
  glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &caps->max_renderbuffer_size);
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &caps->max_texture_image_units);
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &caps->max_texture_size);
  glGetIntegerv(GL_MAX_VARYING_VECTORS, &caps->max_varying_vectors);
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &caps->max_vertex_attribs);
  glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,
                &caps->max_vertex_texture_image_units);
  glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS,
                &caps->max_vertex_uniform_vectors);
  {
    GLint dims[2] = {0, 0};
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, dims);
    caps->max_viewport_width = dims[0];
    caps->max_viewport_height = dims[1];
  }
  glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS,
                &caps->num_compressed_texture_formats);
  glGetIntegerv(GL_NUM_SHADER_BINARY_FORMATS, &caps->num_shader_binary_formats);

  if (feature_info->IsWebGL2OrES3Context()) {
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &caps->max_3d_texture_size);
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &caps->max_array_texture_layers);
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &caps->max_color_attachments);
    glGetInteger64v(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS,
                    &caps->max_combined_fragment_uniform_components);
    glGetIntegerv(GL_MAX_COMBINED_UNIFORM_BLOCKS,
                  &caps->max_combined_uniform_blocks);
    glGetInteger64v(GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS,
                    &caps->max_combined_vertex_uniform_components);
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &caps->max_draw_buffers);
    glGetInteger64v(GL_MAX_ELEMENT_INDEX, &caps->max_element_index);
    glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &caps->max_elements_indices);
    glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &caps->max_elements_vertices);
    glGetIntegerv(GL_MAX_FRAGMENT_INPUT_COMPONENTS,
                  &caps->max_fragment_input_components);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_BLOCKS,
                  &caps->max_fragment_uniform_blocks);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS,
                  &caps->max_fragment_uniform_components);
    glGetIntegerv(GL_MAX_PROGRAM_TEXEL_OFFSET, &caps->max_program_texel_offset);
    glGetInteger64v(GL_MAX_SERVER_WAIT_TIMEOUT, &caps->max_server_wait_timeout);
    glGetFloatv(GL_MAX_TEXTURE_LOD_BIAS, &caps->max_texture_lod_bias);
    glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS,
                  &caps->max_transform_feedback_interleaved_components);
    glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS,
                  &caps->max_transform_feedback_separate_attribs);
    glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS,
                  &caps->max_transform_feedback_separate_components);
    glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &caps->max_uniform_block_size);
    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS,
                  &caps->max_uniform_buffer_bindings);
    glGetIntegerv(GL_MAX_VARYING_COMPONENTS, &caps->max_varying_components);
    glGetIntegerv(GL_MAX_VERTEX_OUTPUT_COMPONENTS,
                  &caps->max_vertex_output_components);
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_BLOCKS,
                  &caps->max_vertex_uniform_blocks);
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS,
                  &caps->max_vertex_uniform_components);
    glGetIntegerv(GL_MIN_PROGRAM_TEXEL_OFFSET, &caps->min_program_texel_offset);
    glGetIntegerv(GL_NUM_EXTENSIONS, &caps->num_extensions);
    glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS,
                  &caps->num_program_binary_formats);
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT,
                  &caps->uniform_buffer_offset_alignment);
    caps->major_version = 3;
    caps->minor_version = 0;
  }
  if (feature_info->feature_flags().multisampled_render_to_texture ||
      feature_info->feature_flags().chromium_framebuffer_multisample ||
      feature_info->IsWebGL2OrES3Context()) {
    glGetIntegerv(GL_MAX_SAMPLES, &caps->max_samples);
  }
}

bool CheckUniqueAndNonNullIds(GLsizei n, const GLuint* client_ids) {
  if (n <= 0)
    return true;
  std::unordered_set<uint32_t> unique_ids(client_ids, client_ids + n);
  return (unique_ids.size() == static_cast<size_t>(n)) &&
         (unique_ids.find(0) == unique_ids.end());
}

const char* GetServiceVersionString(const FeatureInfo* feature_info) {
  if (feature_info->IsWebGL2OrES3Context())
    return "OpenGL ES 3.0 Chromium";
  else
    return "OpenGL ES 2.0 Chromium";
}

const char* GetServiceShadingLanguageVersionString(
    const FeatureInfo* feature_info) {
  if (feature_info->IsWebGL2OrES3Context())
    return "OpenGL ES GLSL ES 3.0 Chromium";
  else
    return "OpenGL ES GLSL ES 1.0 Chromium";
}

static void APIENTRY LogGLDebugMessage(GLenum source,
                                       GLenum type,
                                       GLuint id,
                                       GLenum severity,
                                       GLsizei length,
                                       const GLchar* message,
                                       GLvoid* user_param) {
  std::string id_string = GLES2Util::GetStringEnum(id);
  Logger* error_logger = static_cast<Logger*>(user_param);
  if (type == GL_DEBUG_TYPE_ERROR && source == GL_DEBUG_SOURCE_API) {
    error_logger->LogMessage(__FILE__, __LINE__,
                             " " + id_string + ": " + message);
  } else {
    error_logger->LogMessage(
        __FILE__, __LINE__,
        std::string("GL Driver Message (") + GetDebugSourceString(source) +
            ", " + GetDebugTypeString(type) + ", " + id_string + ", " +
            GetDebugSeverityString(severity) + "): " + message);
  }
}

void InitializeGLDebugLogging(bool log_non_errors, Logger* error_logger) {
  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

  glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE,
                        0, nullptr, GL_TRUE);

  if (log_non_errors) {
    // Enable logging of medium and high severity messages
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_HIGH, 0,
                          nullptr, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_MEDIUM,
                          0, nullptr, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW, 0,
                          nullptr, GL_FALSE);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                          GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
  }

  glDebugMessageCallback(&LogGLDebugMessage, error_logger);
}

bool ValidContextLostReason(GLenum reason) {
  switch (reason) {
    case GL_NO_ERROR:
    case GL_GUILTY_CONTEXT_RESET_ARB:
    case GL_INNOCENT_CONTEXT_RESET_ARB:
    case GL_UNKNOWN_CONTEXT_RESET_ARB:
      return true;
    default:
      return false;
  }
}

error::ContextLostReason GetContextLostReasonFromResetStatus(
    GLenum reset_status) {
  switch (reset_status) {
    case GL_NO_ERROR:
      // TODO(kbr): improve the precision of the error code in this case.
      // Consider delegating to context for error code if MakeCurrent fails.
      return error::kUnknown;
    case GL_GUILTY_CONTEXT_RESET_ARB:
      return error::kGuilty;
    case GL_INNOCENT_CONTEXT_RESET_ARB:
      return error::kInnocent;
    case GL_UNKNOWN_CONTEXT_RESET_ARB:
      return error::kUnknown;
  }

  NOTREACHED();
  return error::kUnknown;
}

}  // namespace gles2
}  // namespace gpu
