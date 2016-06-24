// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gles2_cmd_apply_framebuffer_attachment_cmaa_intel.h"

#include "base/logging.h"
#include "gpu/command_buffer/service/framebuffer_manager.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {

ApplyFramebufferAttachmentCMAAINTELResourceManager::
    ApplyFramebufferAttachmentCMAAINTELResourceManager()
    : initialized_(false),
      textures_initialized_(false),
      is_in_gamma_correct_mode_(false),
      supports_usampler_(true),
      supports_r8_image_(true),
      supports_r8_read_format_(true),
      is_gles31_compatible_(false),
      frame_id_(0),
      width_(0),
      height_(0),
      copy_to_framebuffer_shader_(0),
      copy_to_image_shader_(0),
      edges0_shader_(0),
      edges1_shader_(0),
      edges_combine_shader_(0),
      process_and_apply_shader_(0),
      debug_display_edges_shader_(0),
      cmaa_framebuffer_(0),
      copy_framebuffer_(0),
      rgba8_texture_(0),
      working_color_texture_(0),
      edges0_texture_(0),
      edges1_texture_(0),
      mini4_edge_texture_(0),
      mini4_edge_depth_texture_(0),
      edges1_shader_result_texture_float4_slot1_(0),
      edges1_shader_result_texture_(0),
      edges_combine_shader_result_texture_float4_slot1_(0),
      process_and_apply_shader_result_texture_float4_slot1_(0),
      edges_combine_shader_result_texture_slot2_(0),
      copy_to_image_shader_outTexture_(0) {}

ApplyFramebufferAttachmentCMAAINTELResourceManager::
    ~ApplyFramebufferAttachmentCMAAINTELResourceManager() {
  Destroy();
}

void ApplyFramebufferAttachmentCMAAINTELResourceManager::Initialize(
    gles2::GLES2Decoder* decoder) {
  DCHECK(decoder);
  is_gles31_compatible_ =
      decoder->GetGLContext()->GetVersionInfo()->IsAtLeastGLES(3, 1);

  copy_to_image_shader_ = CreateProgram("", vert_str_, copy_frag_str_);
  copy_to_framebuffer_shader_ =
      CreateProgram("#define OUT_FBO 1\n", vert_str_, copy_frag_str_);

  // Check if RGBA8UI is supported as an FBO colour target with depth.
  // If not supported, GLSL needs to convert the data to/from float so there is
  // a small extra cost.
  {
    GLuint rgba8ui_texture = 0, depth_texture = 0;
    glGenTextures(1, &rgba8ui_texture);
    glBindTexture(GL_TEXTURE_2D, rgba8ui_texture);
    glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8UI, 4, 4);

    glGenTextures(1, &depth_texture);
    glBindTexture(GL_TEXTURE_2D, depth_texture);
    glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, 4, 4);

    // Create the FBO
    GLuint rgba8ui_framebuffer = 0;
    glGenFramebuffersEXT(1, &rgba8ui_framebuffer);
    glBindFramebufferEXT(GL_FRAMEBUFFER, rgba8ui_framebuffer);

    // Bind to the FBO to test support
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, rgba8ui_texture, 0);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_TEXTURE_2D, depth_texture, 0);
    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);

    supports_usampler_ = (status == GL_FRAMEBUFFER_COMPLETE);

    glDeleteFramebuffersEXT(1, &rgba8ui_framebuffer);
    glDeleteTextures(1, &rgba8ui_texture);
    glDeleteTextures(1, &depth_texture);
  }

  // Check to see if R8 images are supported
  // If not supported, images are bound as R32F for write targets, not R8.
  {
    GLuint r8_texture = 0;
    glGenTextures(1, &r8_texture);
    glBindTexture(GL_TEXTURE_2D, r8_texture);
    glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_R8, 4, 4);

    glGetError();  // reset all previous errors
    glBindImageTextureEXT(0, r8_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);
    if (glGetError() != GL_NO_ERROR)
      supports_r8_image_ = false;

    glDeleteTextures(1, &r8_texture);
  }

  // Check if R8 GLSL read formats are supported.
  // If not supported, r32f is used instead.
  {
    const char shader_source[] =
        "layout(r8) restrict writeonly uniform highp image2D g_r8Image;     \n"
        "void main()                                                        \n"
        "{                                                                  \n"
        "    imageStore(g_r8Image, ivec2(0, 0), vec4(1.0, 0.0, 0.0, 0.0));  \n"
        "}                                                                  \n";

    GLuint shader = CreateShader(GL_FRAGMENT_SHADER, "", shader_source);
    supports_r8_read_format_ = (shader != 0);
    if (shader != 0) {
      glDeleteShader(shader);
    }
  }

  VLOG(1) << "ApplyFramebufferAttachmentCMAAINTEL: "
          << "Supports USampler is " << (supports_usampler_ ? "true" : "false");
  VLOG(1) << "ApplyFramebufferAttachmentCMAAINTEL: "
          << "Supports R8 Images is "
          << (supports_r8_image_ ? "true" : "false");
  VLOG(1) << "ApplyFramebufferAttachmentCMAAINTEL: "
          << "Supports R8 Read Format is "
          << (supports_r8_read_format_ ? "true" : "false");

  // Create the shaders
  std::ostringstream defines, edge1, edge2, combineEdges, blur, displayEdges,
      cmaa_frag;

  cmaa_frag << cmaa_frag_s1_ << cmaa_frag_s2_;
  std::string cmaa_frag_string = cmaa_frag.str();
  const char* cmaa_frag_c_str = cmaa_frag_string.c_str();

  if (supports_usampler_) {
    defines << "#define SUPPORTS_USAMPLER2D\n";
  }

  if (is_in_gamma_correct_mode_) {
    defines << "#define IN_GAMMA_CORRECT_MODE\n";
  }

  if (supports_r8_read_format_) {
    defines << "#define EDGE_READ_FORMAT r8\n";
  } else {
    defines << "#define EDGE_READ_FORMAT r32f\n";
  }

  displayEdges << defines.str() << "#define DISPLAY_EDGES\n";
  debug_display_edges_shader_ =
      CreateProgram(displayEdges.str().c_str(), vert_str_, cmaa_frag_c_str);

  edge1 << defines.str() << "#define DETECT_EDGES1\n";
  edges0_shader_ =
      CreateProgram(edge1.str().c_str(), vert_str_, cmaa_frag_c_str);

  edge2 << defines.str() << "#define DETECT_EDGES2\n";
  edges1_shader_ =
      CreateProgram(edge2.str().c_str(), vert_str_, cmaa_frag_c_str);

  combineEdges << defines.str() << "#define COMBINE_EDGES\n";
  edges_combine_shader_ =
      CreateProgram(combineEdges.str().c_str(), vert_str_, cmaa_frag_c_str);

  blur << defines.str() << "#define BLUR_EDGES\n";
  process_and_apply_shader_ =
      CreateProgram(blur.str().c_str(), vert_str_, cmaa_frag_c_str);

  edges1_shader_result_texture_float4_slot1_ =
      glGetUniformLocation(edges0_shader_, "g_resultTextureFlt4Slot1");
  edges1_shader_result_texture_ =
      glGetUniformLocation(edges1_shader_, "g_resultTexture");
  edges_combine_shader_result_texture_float4_slot1_ =
      glGetUniformLocation(edges_combine_shader_, "g_resultTextureFlt4Slot1");
  edges_combine_shader_result_texture_slot2_ =
      glGetUniformLocation(edges_combine_shader_, "g_resultTextureSlot2");
  process_and_apply_shader_result_texture_float4_slot1_ = glGetUniformLocation(
      process_and_apply_shader_, "g_resultTextureFlt4Slot1");
  copy_to_image_shader_outTexture_ =
      glGetUniformLocation(copy_to_image_shader_, "outTexture");

  initialized_ = true;
}

void ApplyFramebufferAttachmentCMAAINTELResourceManager::Destroy() {
  if (!initialized_)
    return;

  ReleaseTextures();

  glDeleteProgram(copy_to_image_shader_);
  glDeleteProgram(copy_to_framebuffer_shader_);
  glDeleteProgram(process_and_apply_shader_);
  glDeleteProgram(edges_combine_shader_);
  glDeleteProgram(edges1_shader_);
  glDeleteProgram(edges0_shader_);
  glDeleteProgram(debug_display_edges_shader_);

  initialized_ = false;
}

// Apply CMAA(Conservative Morphological Anti-Aliasing) algorithm to the
// color attachments of currently bound draw framebuffer.
// Reference GL_INTEL_framebuffer_CMAA for details.
void ApplyFramebufferAttachmentCMAAINTELResourceManager::
    ApplyFramebufferAttachmentCMAAINTEL(gles2::GLES2Decoder* decoder,
                                        gles2::Framebuffer* framebuffer) {
  DCHECK(decoder);
  DCHECK(initialized_);
  if (!framebuffer)
    return;

  GLuint last_framebuffer = framebuffer->service_id();

  // Process each color attachment of the current draw framebuffer.
  uint32_t max_draw_buffers = decoder->GetContextGroup()->max_draw_buffers();
  for (uint32_t i = 0; i < max_draw_buffers; i++) {
    const gles2::Framebuffer::Attachment* attachment =
        framebuffer->GetAttachment(GL_COLOR_ATTACHMENT0 + i);
    if (attachment && attachment->IsTextureAttachment()) {
      // Get the texture info.
      GLuint source_texture_client_id = attachment->object_name();
      GLuint source_texture = 0;
      if (!decoder->GetServiceTextureId(source_texture_client_id,
                                        &source_texture))
        continue;
      GLsizei width = attachment->width();
      GLsizei height = attachment->height();
      GLenum internal_format = attachment->internal_format();

      // Resize internal structures - only if needed.
      OnSize(width, height);

      // CMAA internally expects GL_RGBA8 textures.
      // Process using a GL_RGBA8 copy if this is not the case.
      bool do_copy = internal_format != GL_RGBA8;

      // Copy source_texture to rgba8_texture_
      if (do_copy) {
        CopyTexture(source_texture, rgba8_texture_, false);
      }

      // CMAA Effect
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, last_framebuffer);
      if (do_copy) {
        ApplyCMAAEffectTexture(rgba8_texture_, rgba8_texture_);
      } else {
        ApplyCMAAEffectTexture(source_texture, source_texture);
      }

      // Copy rgba8_texture_ to source_texture
      if (do_copy) {
        // Move source_texture to the first color attachment of the copy fbo.
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, last_framebuffer);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                                  GL_TEXTURE_2D, 0, 0);
        glBindFramebufferEXT(GL_FRAMEBUFFER, copy_framebuffer_);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_TEXTURE_2D, source_texture, 0);

        CopyTexture(rgba8_texture_, source_texture, true);

        // Restore color attachments
        glBindFramebufferEXT(GL_FRAMEBUFFER, copy_framebuffer_);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_TEXTURE_2D, rgba8_texture_, 0);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, last_framebuffer);
        glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                                  GL_TEXTURE_2D, source_texture, 0);
      }
    }
  }

  // Restore state
  decoder->RestoreAllAttributes();
  decoder->RestoreTextureUnitBindings(0);
  decoder->RestoreTextureUnitBindings(1);
  decoder->RestoreActiveTexture();
  decoder->RestoreProgramBindings();
  decoder->RestoreBufferBindings();
  decoder->RestoreFramebufferBindings();
  decoder->RestoreGlobalState();
}

void ApplyFramebufferAttachmentCMAAINTELResourceManager::ApplyCMAAEffectTexture(
    GLuint source_texture,
    GLuint dest_texture) {
  frame_id_++;

  GLuint edge_texture_a;
  GLuint edge_texture_b;

  // Flip flop - One pass clears the texture that needs clearing for the other
  // one (actually it's only important that it clears the highest bit)
  if ((frame_id_ % 2) == 0) {
    edge_texture_a = edges0_texture_;
    edge_texture_b = edges1_texture_;
  } else {
    edge_texture_a = edges1_texture_;
    edge_texture_b = edges0_texture_;
  }

  // Setup the main fbo
  glBindFramebufferEXT(GL_FRAMEBUFFER, cmaa_framebuffer_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            mini4_edge_texture_, 0);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                            mini4_edge_depth_texture_, 0);
#if DCHECK_IS_ON()
  GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    DLOG(ERROR) << "ApplyFramebufferAttachmentCMAAINTEL: "
                << "Incomplete framebuffer.";
    Destroy();
    return;
  }
#endif

  // Setup the viewport to match the fbo
  glViewport(0, 0, (width_ + 1) / 2, (height_ + 1) / 2);
  glEnable(GL_DEPTH_TEST);

  // Detect edges Pass 0
  //   - For every pixel detect edges to the right and down and output depth
  //   mask where edges detected (1 - far, for detected, 0-near for empty
  //   pixels)

  // Inputs
  //  g_screenTexture                     source_texture               tex0
  // Outputs
  //  gl_FragDepth                        mini4_edge_depth_texture_    fbo.depth
  //  out uvec4 outEdges                  mini4_edge_texture_          fbo.col
  //  image2D g_resultTextureFlt4Slot1    working_color_texture_       image1
  GLenum edge_format = supports_r8_image_ ? GL_R8 : GL_R32F;

  {
    glUseProgram(edges0_shader_);
    glUniform1f(0, 1.0f);
    glUniform2f(1, 1.0f / width_, 1.0f / height_);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_ALWAYS);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    if (!is_gles31_compatible_) {
      glUniform1i(edges1_shader_result_texture_float4_slot1_, 1);
    }
    glBindImageTextureEXT(1, working_color_texture_, 0, GL_FALSE, 0,
                          GL_WRITE_ONLY, GL_RGBA8);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glDrawArrays(GL_TRIANGLES, 0, 3);
  }

  // Detect edges Pass 1 (finish the previous pass edge processing).
  // Do the culling of non-dominant local edges (leave mainly locally dominant
  // edges) and merge Right and Bottom edges into TopRightBottomLeft

  // Inputs
  //  g_src0Texture4Uint                  mini4_edge_texture_          tex1
  // Outputs
  //  image2D g_resultTexture             edge_texture_b               image0
  {
    glUseProgram(edges1_shader_);
    glUniform1f(0, 0.0f);
    glUniform2f(1, 1.0f / width_, 1.0f / height_);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LESS);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    if (!is_gles31_compatible_) {
      glUniform1i(edges1_shader_result_texture_, 0);
    }
    glBindImageTextureEXT(0, edge_texture_b, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                          edge_format);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, mini4_edge_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glDrawArrays(GL_TRIANGLES, 0, 3);
  }

  //  - Combine RightBottom (.xy) edges from previous pass into
  //    RightBottomLeftTop (.xyzw) edges and output it into the mask (have to
  //    fill in the whole buffer including empty ones for the line length
  //    detection to work correctly).
  //  - On all pixels with any edge, input buffer into a temporary color buffer
  //    needed for correct blending in the next pass (other pixels not needed
  //    so not copied to avoid bandwidth use).
  //  - On all pixels with 2 or more edges output positive depth mask for the
  //    next pass.

  // Inputs
  //  g_src0TextureFlt                    edge_texture_b               tex1 //ps
  // Outputs
  //  image2D g_resultTextureSlot2        edge_texture_a               image2
  //  gl_FragDepth                        mini4_edge_texture_          fbo.depth
  {
    // Combine edges: each pixel will now contain info on all (top, right,
    // bottom, left) edges; also create depth mask as above depth and mark
    // potential Z sAND also copy source color data but only on edge pixels
    glUseProgram(edges_combine_shader_);
    glUniform1f(0, 1.0f);
    glUniform2f(1, 1.0f / width_, 1.0f / height_);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_ALWAYS);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    if (!is_gles31_compatible_) {
      glUniform1i(edges_combine_shader_result_texture_slot2_, 2);
    }
    glBindImageTextureEXT(2, edge_texture_a, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                          edge_format);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, edge_texture_b);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glDrawArrays(GL_TRIANGLES, 0, 3);
  }

  // Using depth mask and [earlydepthstencil] to work on pixels with 2, 3, 4
  // edges:
  //    - First blend simple blur map for 2,3,4 edge pixels
  //    - Then do the lines (line length counter -should- guarantee no overlap
  //      with other pixels - pixels with 1 edge are excluded in the previous
  //      pass and the pixels with 2 parallel edges are excluded in the simple
  //      blur)

  // Inputs
  //  g_screenTexture                      working_color_texture_      tex0
  //  g_src0TextureFlt                     edge_texture_a              tex1 //ps
  //  sampled
  // Outputs
  //  g_resultTextureFlt4Slot1             dest_texture                image1
  //  gl_FragDepth                         mini4_edge_texture_         fbo.depth
  {
    glUseProgram(process_and_apply_shader_);
    glUniform1f(0, 0.0f);
    glUniform2f(1, 1.0f / width_, 1.0f / height_);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LESS);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    if (!is_gles31_compatible_) {
      glUniform1i(process_and_apply_shader_result_texture_float4_slot1_, 1);
    }
    glBindImageTextureEXT(1, dest_texture, 0, GL_FALSE, 0, GL_WRITE_ONLY,
                          GL_RGBA8);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, working_color_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, edge_texture_a);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glDrawArrays(GL_TRIANGLES, 0, 3);
  }

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glActiveTexture(GL_TEXTURE0);
}

void ApplyFramebufferAttachmentCMAAINTELResourceManager::OnSize(GLint width,
                                                                GLint height) {
  if (height_ == height && width_ == width)
    return;

  ReleaseTextures();

  height_ = height;
  width_ = width;

  glGenFramebuffersEXT(1, &copy_framebuffer_);
  glGenTextures(1, &rgba8_texture_);
  glBindTexture(GL_TEXTURE_2D, rgba8_texture_);
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);

  // Edges texture - R8
  // OpenGLES has no single component 8/16-bit image support, so needs to be R32
  // Although CHT does support R8.
  GLenum edge_format = supports_r8_image_ ? GL_R8 : GL_R32F;
  glGenTextures(1, &edges0_texture_);
  glBindTexture(GL_TEXTURE_2D, edges0_texture_);
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, edge_format, width, height);

  glGenTextures(1, &edges1_texture_);
  glBindTexture(GL_TEXTURE_2D, edges1_texture_);
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, edge_format, width, height);

  // Color working texture - RGBA8
  glGenTextures(1, &working_color_texture_);
  glBindTexture(GL_TEXTURE_2D, working_color_texture_);
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);

  // Half*half compressed 4-edge-per-pixel texture - RGBA8
  glGenTextures(1, &mini4_edge_texture_);
  glBindTexture(GL_TEXTURE_2D, mini4_edge_texture_);
  GLenum format = GL_RGBA8UI;
  if (!supports_usampler_) {
    format = GL_RGBA8;
  }
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, format, (width + 1) / 2,
                    (height + 1) / 2);

  // Depth
  glGenTextures(1, &mini4_edge_depth_texture_);
  glBindTexture(GL_TEXTURE_2D, mini4_edge_depth_texture_);
  glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, (width + 1) / 2,
                    (height + 1) / 2);

  // Create the FBO
  glGenFramebuffersEXT(1, &cmaa_framebuffer_);
  glBindFramebufferEXT(GL_FRAMEBUFFER, cmaa_framebuffer_);

  // We need to clear the textures before they are first used.
  // The algorithm self-clears them later.
  glViewport(0, 0, width_, height_);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

  glBindFramebufferEXT(GL_FRAMEBUFFER, cmaa_framebuffer_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            edges0_texture_, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            edges1_texture_, 0);
  glClear(GL_COLOR_BUFFER_BIT);

  textures_initialized_ = true;
}

void ApplyFramebufferAttachmentCMAAINTELResourceManager::ReleaseTextures() {
  if (textures_initialized_) {
    glDeleteFramebuffersEXT(1, &copy_framebuffer_);
    glDeleteFramebuffersEXT(1, &cmaa_framebuffer_);
    glDeleteTextures(1, &rgba8_texture_);
    glDeleteTextures(1, &edges0_texture_);
    glDeleteTextures(1, &edges1_texture_);
    glDeleteTextures(1, &mini4_edge_texture_);
    glDeleteTextures(1, &mini4_edge_depth_texture_);
    glDeleteTextures(1, &working_color_texture_);
  }
  textures_initialized_ = false;
}

void ApplyFramebufferAttachmentCMAAINTELResourceManager::CopyTexture(
    GLint source,
    GLint dest,
    bool via_fbo) {
  glViewport(0, 0, width_, height_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, source);

  if (!via_fbo) {
    glUseProgram(copy_to_image_shader_);
    if (!is_gles31_compatible_) {
      glUniform1i(copy_to_image_shader_outTexture_, 0);
    }
    glBindImageTextureEXT(0, dest, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
  } else {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glUseProgram(copy_to_framebuffer_shader_);
  }

  glDrawArrays(GL_TRIANGLES, 0, 3);
  glUseProgram(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint ApplyFramebufferAttachmentCMAAINTELResourceManager::CreateProgram(
    const char* defines,
    const char* vs_source,
    const char* fs_source) {
  GLuint program = glCreateProgram();

  GLuint vs = CreateShader(GL_VERTEX_SHADER, defines, vs_source);
  GLuint fs = CreateShader(GL_FRAGMENT_SHADER, defines, fs_source);

  glAttachShader(program, vs);
  glDeleteShader(vs);
  glAttachShader(program, fs);
  glDeleteShader(fs);

  glLinkProgram(program);
  GLint link_status;
  glGetProgramiv(program, GL_LINK_STATUS, &link_status);

  if (link_status == 0) {
#if DCHECK_IS_ON()
    GLint info_log_length;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
    std::vector<GLchar> info_log(info_log_length);
    glGetProgramInfoLog(program, static_cast<GLsizei>(info_log.size()), NULL,
                        &info_log[0]);
    DLOG(ERROR) << "ApplyFramebufferAttachmentCMAAINTEL: "
                << "program link failed: " << &info_log[0];
#endif
    glDeleteProgram(program);
    program = 0;
  }

  return program;
}

GLuint ApplyFramebufferAttachmentCMAAINTELResourceManager::CreateShader(
    GLenum type,
    const char* defines,
    const char* source) {
  GLuint shader = glCreateShader(type);

  const char header_es31[] =
      "#version 310 es                                                      \n";
  const char header_gl30[] =
      "#version 130                                                         \n"
      "#extension GL_ARB_shading_language_420pack  : require                \n"
      "#extension GL_ARB_texture_gather            : require                \n"
      "#extension GL_ARB_explicit_uniform_location : require                \n"
      "#extension GL_ARB_explicit_attrib_location  : require                \n"
      "#extension GL_ARB_shader_image_load_store   : require                \n";

  const char* header = NULL;
  if (is_gles31_compatible_) {
    header = header_es31;
  } else {
    header = header_gl30;
  }

  const char* source_array[4] = {header, defines, "\n", source};
  glShaderSource(shader, 4, source_array, NULL);

  glCompileShader(shader);

  GLint compile_result;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_result);
  if (compile_result == 0) {
#if DCHECK_IS_ON()
    GLint info_log_length;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
    std::vector<GLchar> info_log(info_log_length);
    glGetShaderInfoLog(shader, static_cast<GLsizei>(info_log.size()), NULL,
                       &info_log[0]);
    DLOG(ERROR) << "ApplyFramebufferAttachmentCMAAINTEL: "
                << "shader compilation failed: "
                << (type == GL_VERTEX_SHADER
                        ? "GL_VERTEX_SHADER"
                        : (type == GL_FRAGMENT_SHADER ? "GL_FRAGMENT_SHADER"
                                                      : "UNKNOWN_SHADER"))
                << " shader compilation failed: " << &info_log[0];
#endif
    glDeleteShader(shader);
    shader = 0;
  }

  return shader;
}

// Shaders used in the CMAA algorithm.
const char ApplyFramebufferAttachmentCMAAINTELResourceManager::vert_str_[] =
    "precision highp float;                                                 \n"
    "layout(location = 0) uniform float g_Depth;                            \n"
    "// No input data.                                                      \n"
    "// Verts are autogenerated.                                            \n"
    "//                                                                     \n"
    "// vertexID 0,1,2 should generate                                      \n"
    "// POS: (-1,-1), (+3,-1), (-1,+3)                                      \n"
    "//                                                                     \n"
    "// This generates a triangle that completely covers the -1->1 viewport \n"
    "//                                                                     \n"
    "void main()                                                            \n"
    "{                                                                      \n"
    "   float x = -1.0 + float((gl_VertexID & 1) << 2);                     \n"
    "   float y = -1.0 + float((gl_VertexID & 2) << 1);                     \n"
    "   gl_Position = vec4(x, y, g_Depth, 1.0);                             \n"
    "}                                                                      \n"
    "                                                                       \n";

const char ApplyFramebufferAttachmentCMAAINTELResourceManager::cmaa_frag_s1_[] =
    "precision highp float;                                                 \n"
    "precision highp int;                                                   \n"
    "                                                                       \n"
    "#define SETTINGS_ALLOW_SHORT_Zs 1                                      \n"
    "#define EDGE_DETECT_THRESHOLD 13.0f                                    \n"
    "                                                                       \n"
    "#define saturate(x) clamp((x), 0.0, 1.0)                               \n"
    "                                                                       \n"
    "// bind to location 0                                                  \n"
    "layout(location = 0) uniform float g_Depth;                            \n"
    "// bind to a uniform buffer bind point 0                               \n"
    "layout(location = 1) uniform vec2 g_OneOverScreenSize;                 \n"
    "#ifndef EDGE_DETECT_THRESHOLD                                          \n"
    "layout(location = 2) uniform float g_ColorThreshold;                   \n"
    "#endif                                                                 \n"
    "                                                                       \n"
    "#ifdef SUPPORTS_USAMPLER2D                                             \n"
    "#define USAMPLER usampler2D                                            \n"
    "#define UVEC4 uvec4                                                    \n"
    "#define LOAD_UINT(arg) arg                                             \n"
    "#define STORE_UVEC4(arg) arg                                           \n"
    "#else                                                                  \n"
    "#define USAMPLER sampler2D                                             \n"
    "#define UVEC4 vec4                                                     \n"
    "#define LOAD_UINT(arg) uint(arg * 255.0f)                              \n"
    "#define STORE_UVEC4(arg) vec4(float(arg.x) / 255.0f,                   \n"
    "                              float(arg.y) / 255.0f,                   \n"
    "                              float(arg.z) / 255.0f,                   \n"
    "                              float(arg.w) / 255.0f)                   \n"
    "#endif                                                                 \n"
    "                                                                       \n"
    "// bind to texture stage 0/1                                           \n"
    "layout(binding = 0) uniform highp sampler2D g_screenTexture;           \n"
    "layout(binding = 1) uniform highp sampler2D g_src0TextureFlt;          \n"
    "layout(binding = 1) uniform highp USAMPLER g_src0Texture4Uint;         \n"
    "                                                                       \n"
    "// bind to image stage 0/1/2                                           \n"
    "#ifdef GL_ES                                                           \n"
    "layout(binding = 0, EDGE_READ_FORMAT) restrict writeonly uniform highp \n"
    "    image2D g_resultTexture;                                           \n"
    "layout(binding = 1, rgba8) restrict writeonly uniform highp            \n"
    "    image2D g_resultTextureFlt4Slot1;                                  \n"
    "layout(binding = 2, EDGE_READ_FORMAT) restrict writeonly uniform highp \n"
    "    image2D g_resultTextureSlot2;                                      \n"
    "#else                                                                  \n"
    "layout(EDGE_READ_FORMAT) restrict writeonly uniform highp              \n"
    "    image2D g_resultTexture;                                           \n"
    "layout(rgba8) restrict writeonly uniform highp                         \n"
    "    image2D g_resultTextureFlt4Slot1;                                  \n"
    "layout(EDGE_READ_FORMAT) restrict writeonly uniform highp              \n"
    "    image2D g_resultTextureSlot2;                                      \n"
    "#endif                                                                 \n"
    "                                                                       \n"
    "// Constants                                                           \n"
    "const vec4 c_lumWeights = vec4(0.2126f, 0.7152f, 0.0722f, 0.0000f);    \n"
    "                                                                       \n"
    "#ifdef EDGE_DETECT_THRESHOLD                                           \n"
    "const float c_ColorThreshold = 1.0f / EDGE_DETECT_THRESHOLD;           \n"
    "#endif                                                                 \n"
    "                                                                       \n"
    "// Must be even number; Will work with ~16 pretty good too for         \n"
    "// additional performance, or with ~64 for highest quality.            \n"
    "const int c_maxLineLength = 64;                                        \n"
    "                                                                       \n"
    "const vec4 c_edgeDebugColours[5] = vec4[5](vec4(0.5, 0.5, 0.5, 0.4),   \n"
    "                                           vec4(1.0, 0.1, 1.0, 0.8),   \n"
    "                                           vec4(0.9, 0.0, 0.0, 0.8),   \n"
    "                                           vec4(0.0, 0.9, 0.0, 0.8),   \n"
    "                                           vec4(0.0, 0.0, 0.9, 0.8));  \n"
    "                                                                       \n"
    "// this isn't needed if colour UAV is _SRGB but that doesn't work      \n"
    "// everywhere                                                          \n"
    "#ifdef IN_GAMMA_CORRECT_MODE                                           \n"
    "///////////////////////////////////////////////////////////////////////\n"
    "//                                                                     \n"
    "// SRGB Helper Functions taken from D3DX_DXGIFormatConvert.inl         \n"
    "float D3DX_FLOAT_to_SRGB(float val) {                                  \n"
    "  if (val < 0.0031308f)                                                \n"
    "    val *= 12.92f;                                                     \n"
    "  else {                                                               \n"
    "    val = 1.055f * pow(val, 1.0f / 2.4f) - 0.055f;                     \n"
    "  }                                                                    \n"
    "  return val;                                                          \n"
    "}                                                                      \n"
    "//                                                                     \n"
    "vec3 D3DX_FLOAT3_to_SRGB(vec3 val) {                                   \n"
    "  vec3 outVal;                                                         \n"
    "  outVal.x = D3DX_FLOAT_to_SRGB(val.x);                                \n"
    "  outVal.y = D3DX_FLOAT_to_SRGB(val.y);                                \n"
    "  outVal.z = D3DX_FLOAT_to_SRGB(val.z);                                \n"
    "  return outVal;                                                       \n"
    "}                                                                      \n"
    "//                                                                     \n"
    "///////////////////////////////////////////////////////////////////////\n"
    "#endif  // IN_GAMMA_CORRECT_MODE                                       \n"
    "                                                                       \n"
    "// how .rgba channels from the edge texture maps to pixel edges:       \n"
    "//                                                                     \n"
    "//                   A - 0x08                                          \n"
    "//              |¯¯¯¯¯¯¯¯¯|                                            \n"
    "//              |         |                                            \n"
    "//     0x04 - B |  pixel  | R - 0x01                                   \n"
    "//              |         |                                            \n"
    "//              |_________|                                            \n"
    "//                   G - 0x02                                          \n"
    "//                                                                     \n"
    "// (A - there's an edge between us and a pixel above us)               \n"
    "// (R - there's an edge between us and a pixel to the right)           \n"
    "// (G - there's an edge between us and a pixel at the bottom)          \n"
    "// (B - there's an edge between us and a pixel to the left)            \n"
    "                                                                       \n"
    "// Expecting values of 1 and 0 only!                                   \n"
    "uint PackEdge(uvec4 edges) {                                           \n"
    "  return (edges.x << 0u) | (edges.y << 1u) | (edges.z << 2u) |         \n"
    "         (edges.w << 3u);                                              \n"
    "}                                                                      \n"
    "                                                                       \n"
    "uvec4 UnpackEdge(uint value) {                                         \n"
    "  uvec4 ret;                                                           \n"
    "  ret.x = (value & 0x01u) != 0u ? 1u : 0u;                             \n"
    "  ret.y = (value & 0x02u) != 0u ? 1u : 0u;                             \n"
    "  ret.z = (value & 0x04u) != 0u ? 1u : 0u;                             \n"
    "  ret.w = (value & 0x08u) != 0u ? 1u : 0u;                             \n"
    "  return ret;                                                          \n"
    "}                                                                      \n"
    "                                                                       \n"
    "uint PackZ(const uvec2 screenPos, const bool invertedZShape) {         \n"
    "  uint retVal = screenPos.x | (screenPos.y << 15u);                    \n"
    "  if (invertedZShape)                                                  \n"
    "    retVal |= (1u << 30u);                                             \n"
    "  return retVal;                                                       \n"
    "}                                                                      \n"
    "                                                                       \n"
    "void UnpackZ(uint packedZ, out uvec2 screenPos,                        \n"
    "                           out bool invertedZShape)                    \n"
    "{                                                                      \n"
    "  screenPos.x = packedZ & 0x7FFFu;                                     \n"
    "  screenPos.y = (packedZ >> 15u) & 0x7FFFu;                            \n"
    "  invertedZShape = (packedZ >> 30u) == 1u;                             \n"
    "}                                                                      \n"
    "                                                                       \n"
    "uint PackZ(const uvec2 screenPos,                                      \n"
    "           const bool invertedZShape,                                  \n"
    "           const bool horizontal) {                                    \n"
    "  uint retVal = screenPos.x | (screenPos.y << 15u);                    \n"
    "  if (invertedZShape)                                                  \n"
    "    retVal |= (1u << 30u);                                             \n"
    "  if (horizontal)                                                      \n"
    "    retVal |= (1u << 31u);                                             \n"
    "  return retVal;                                                       \n"
    "}                                                                      \n"
    "                                                                       \n"
    "void UnpackZ(uint packedZ,                                             \n"
    "             out uvec2 screenPos,                                      \n"
    "             out bool invertedZShape,                                  \n"
    "             out bool horizontal) {                                    \n"
    "  screenPos.x = packedZ & 0x7FFFu;                                     \n"
    "  screenPos.y = (packedZ >> 15u) & 0x7FFFu;                            \n"
    "  invertedZShape = (packedZ & (1u << 30u)) != 0u;                      \n"
    "  horizontal = (packedZ & (1u << 31u)) != 0u;                          \n"
    "}                                                                      \n"
    "                                                                       \n"
    "vec4 PackBlurAAInfo(ivec2 pixelPos, uint shapeType) {                  \n"
    "  uint packedEdges = uint(                                             \n"
    "      texelFetch(g_src0TextureFlt, pixelPos, 0).r * 255.5);            \n"
    "                                                                       \n"
    "  float retval = float(packedEdges + (shapeType << 4u));               \n"
    "                                                                       \n"
    "  return vec4(retval / 255.0);                                         \n"
    "}                                                                      \n"
    "                                                                       \n"
    "void UnpackBlurAAInfo(float packedValue, out uint edges,               \n"
    "                      out uint shapeType) {                            \n"
    "  uint packedValueInt = uint(packedValue * 255.5);                     \n"
    "  edges = packedValueInt & 0xFu;                                       \n"
    "  shapeType = packedValueInt >> 4u;                                    \n"
    "}                                                                      \n"
    "                                                                       \n"
    "float EdgeDetectColorCalcDiff(vec3 colorA, vec3 colorB) {              \n"
    "#ifdef IN_BGR_MODE                                                     \n"
    "  vec3 LumWeights = c_lumWeights.bgr;                                  \n"
    "#else                                                                  \n"
    "  vec3 LumWeights = c_lumWeights.rgb;                                  \n"
    "#endif                                                                 \n"
    "                                                                       \n"
    "  return dot(abs(colorA.rgb - colorB.rgb), LumWeights);                \n"
    "}                                                                      \n"
    "                                                                       \n"
    "bool EdgeDetectColor(vec3 colorA, vec3 colorB) {                       \n"
    "#ifdef EDGE_DETECT_THRESHOLD                                           \n"
    "  return EdgeDetectColorCalcDiff(colorA, colorB) > c_ColorThreshold;   \n"
    "#else                                                                  \n"
    "  return EdgeDetectColorCalcDiff(colorA, colorB) > g_ColorThreshold;   \n"
    "#endif                                                                 \n"
    "}                                                                      \n"
    "                                                                       \n"
    "void FindLineLength(out int lineLengthLeft,                            \n"
    "                    out int lineLengthRight,                           \n"
    "                    ivec2 screenPos,                                   \n"
    "                    const bool horizontal,                             \n"
    "                    const bool invertedZShape,                         \n"
    "                    const ivec2 stepRight) {                           \n"
    "  // TODO: there must be a cleaner and faster way to get to these -    \n"
    "  // a precalculated array indexing maybe?                             \n"
    "  uint maskLeft, bitsContinueLeft, maskRight, bitsContinueRight;       \n"
    "  {                                                                    \n"
    "  // Horizontal (vertical is the same, just rotated 90º                \n"
    "  // counter-clockwise)                                                \n"
    "  // Inverted Z case:              // Normal Z case:                   \n"
    "  //   __                          // __                               \n"
    "  //  X|                           //  X|                              \n"
    "  // --                            //    --                            \n"
    "  //                                                                   \n"
    "    uint maskTraceLeft, maskTraceRight;                                \n"
    "    uint maskStopLeft, maskStopRight;                                  \n"
    "    if (horizontal) {                                                  \n"
    "      if (invertedZShape) {                                            \n"
    "        maskTraceLeft = 0x02u;  // tracing bottom edge                 \n"
    "        maskTraceRight = 0x08u; // tracing top edge                    \n"
    "      } else {                                                         \n"
    "        maskTraceLeft = 0x08u;  // tracing top edge                    \n"
    "        maskTraceRight = 0x02u; // tracing bottom edge                 \n"
    "      }                                                                \n"
    "      maskStopLeft = 0x01u;  // stop on right edge                     \n"
    "      maskStopRight = 0x04u; // stop on left edge                      \n"
    "    } else {                                                           \n"
    "      if (invertedZShape) {                                            \n"
    "        maskTraceLeft = 0x01u;  // tracing right edge                  \n"
    "        maskTraceRight = 0x04u; // tracing left edge                   \n"
    "      } else {                                                         \n"
    "        maskTraceLeft = 0x04u;  // tracing left edge                   \n"
    "        maskTraceRight = 0x01u; // tracing right edge                  \n"
    "      }                                                                \n"
    "      maskStopLeft = 0x08u;  // stop on top edge                       \n"
    "      maskStopRight = 0x02u; // stop on bottom edge                    \n"
    "    }                                                                  \n"
    "                                                                       \n"
    "    maskLeft = maskTraceLeft | maskStopLeft;                           \n"
    "    bitsContinueLeft = maskTraceLeft;                                  \n"
    "    maskRight = maskTraceRight | maskStopRight;                        \n"
    "    bitsContinueRight = maskTraceRight;                                \n"
    "  }                                                                    \n"
    "///////////////////////////////////////////////////////////////////////\n"
    "                                                                       \n"
    "#ifdef SETTINGS_ALLOW_SHORT_Zs                                         \n"
    "  int i = 1;                                                           \n"
    "#else                                                                  \n"
    "  int i = 2; // starting from 2 because we already know it's at least 2\n"
    "#endif                                                                 \n"
    "  for (; i < c_maxLineLength; i++) {                                   \n"
    "    uint edgeLeft = uint(                                              \n"
    "        texelFetch(g_src0TextureFlt,                                   \n"
    "                   ivec2(screenPos.xy - stepRight * i), 0).r * 255.5); \n"
    "    uint edgeRight = uint(                                             \n"
    "        texelFetch(g_src0TextureFlt,                                   \n"
    "                   ivec2(screenPos.xy + stepRight * (i + 1)),          \n"
    "                   0).r * 255.5);                                      \n"
    "                                                                       \n"
    "    // stop on encountering 'stopping' edge (as defined by masks)      \n"
    "    int stopLeft = (edgeLeft & maskLeft) != bitsContinueLeft ? 1 : 0;  \n"
    "    int stopRight =                                                    \n"
    "        (edgeRight & maskRight) != bitsContinueRight ? 1 : 0;          \n"
    "                                                                       \n"
    "    if (bool(stopLeft) || bool(stopRight)) {                           \n"
    "      lineLengthLeft = 1 + i - stopLeft;                               \n"
    "      lineLengthRight = 1 + i - stopRight;                             \n"
    "      return;                                                          \n"
    "    }                                                                  \n"
    "  }                                                                    \n"
    "  lineLengthLeft = lineLengthRight = i;                                \n"
    "  return;                                                              \n"
    "}                                                                      \n"
    "                                                                       \n"
    "void ProcessDetectedZ(ivec2 screenPos, bool horizontal,                \n"
    "                      bool invertedZShape) {                           \n"
    "  int lineLengthLeft, lineLengthRight;                                 \n"
    "                                                                       \n"
    "  ivec2 stepRight = (horizontal) ? (ivec2(1, 0)) : (ivec2(0, -1));     \n"
    "  vec2 blendDir = (horizontal) ? (vec2(0, -1)) : (vec2(-1, 0));        \n"
    "                                                                       \n"
    "  FindLineLength(lineLengthLeft, lineLengthRight, screenPos,           \n"
    "                 horizontal, invertedZShape, stepRight);               \n"
    "                                                                       \n"
    "  vec2 pixelSize = g_OneOverScreenSize;                                \n"
    "                                                                       \n"
    "  float leftOdd = 0.15 * float(lineLengthLeft % 2);                    \n"
    "  float rightOdd = 0.15 * float(lineLengthRight % 2);                  \n"
    "                                                                       \n"
    "  int loopFrom = -int((lineLengthLeft + 1) / 2) + 1;                   \n"
    "  int loopTo = int((lineLengthRight + 1) / 2);                         \n"
    "                                                                       \n"
    "  float totalLength = float(loopTo - loopFrom) + 1.0 - leftOdd -       \n"
    "                      rightOdd;                                        \n"
    "                                                                       \n"
    "  for (int i = loopFrom; i <= loopTo; i++) {                           \n"
    "    highp ivec2 pixelPos = screenPos + stepRight * i;                  \n"
    "    vec2 pixelPosFlt = vec2(float(pixelPos.x) + 0.5,                   \n"
    "                            float(pixelPos.y) + 0.5);                  \n"
    "                                                                       \n"
    "#ifdef DEBUG_OUTPUT_AAINFO                                             \n"
    "    imageStore(g_resultTextureSlot2, pixelPos,                         \n"
    "               PackBlurAAInfo(pixelPos, 1u));                          \n"
    "#endif                                                                 \n"
    "                                                                       \n"
    "    float m = (float(i) + 0.5 - leftOdd - float(loopFrom)) /           \n"
    "               totalLength;                                            \n"
    "    m = saturate(m);                                                   \n"
    "    float k = m - ((i > 0) ? 1.0 : 0.0);                               \n"
    "    k = (invertedZShape) ? (-k) : (k);                                 \n"
    "                                                                       \n"
    "    vec4 color = textureLod(g_screenTexture,                           \n"
    "                            (pixelPosFlt + blendDir * k) * pixelSize,  \n"
    "                            0.0);                                      \n"
    "                                                                       \n"
    "#ifdef IN_GAMMA_CORRECT_MODE                                           \n"
    "    color.rgb = D3DX_FLOAT3_to_SRGB(color.rgb);                        \n"
    "#endif                                                                 \n"
    "    imageStore(g_resultTextureFlt4Slot1, pixelPos, color);             \n"
    "  }                                                                    \n"
    "}                                                                      \n"
    "                                                                       \n"
    "vec4 CalcDbgDisplayColor(const vec4 blurMap) {                         \n"
    "  vec3 pixelC = vec3(0.0, 0.0, 0.0);                                   \n"
    "  vec3 pixelL = vec3(0.0, 0.0, 1.0);                                   \n"
    "  vec3 pixelT = vec3(1.0, 0.0, 0.0);                                   \n"
    "  vec3 pixelR = vec3(0.0, 1.0, 0.0);                                   \n"
    "  vec3 pixelB = vec3(0.8, 0.8, 0.0);                                   \n"
    "                                                                       \n"
    "  const float centerWeight = 1.0;                                      \n"
    "  float fromBelowWeight = (1.0 / (1.0 - blurMap.x)) - 1.0;             \n"
    "  float fromAboveWeight = (1.0 / (1.0 - blurMap.y)) - 1.0;             \n"
    "  float fromRightWeight = (1.0 / (1.0 - blurMap.z)) - 1.0;             \n"
    "  float fromLeftWeight = (1.0 / (1.0 - blurMap.w)) - 1.0;              \n"
    "                                                                       \n"
    "  float weightSum = centerWeight + dot(vec4(fromBelowWeight,           \n"
    "                                            fromAboveWeight,           \n"
    "                                            fromRightWeight,           \n"
    "                                            fromLeftWeight),           \n"
    "                                       vec4(1, 1, 1, 1));              \n"
    "                                                                       \n"
    "  vec4 pixel;                                                          \n"
    "                                                                       \n"
    "  pixel.rgb = pixelC.rgb + fromAboveWeight * pixelT +                  \n"
    "                           fromBelowWeight * pixelB +                  \n"
    "                           fromLeftWeight * pixelL +                   \n"
    "                           fromRightWeight * pixelR;                   \n"
    "  pixel.rgb /= weightSum;                                              \n"
    "                                                                       \n"
    "  pixel.a = dot(pixel.rgb, vec3(1, 1, 1)) * 100.0;                     \n"
    "                                                                       \n"
    "  return saturate(pixel);                                              \n"
    "}                                                                      \n"
    "                                                                       \n"
    "#ifdef DETECT_EDGES1                                                   \n"
    "layout(location = 0) out UVEC4 outEdges;                               \n"
    "void DetectEdges1() {                                                  \n"
    "  uvec4 outputEdges;                                                   \n"
    "  ivec2 screenPosI = ivec2(gl_FragCoord.xy) * ivec2(2, 2);             \n"
    "                                                                       \n"
    "  // .rgb contains colour, .a contains flag whether to output it to    \n"
    "  // working colour texture                                            \n"
    "  vec4 pixel00 = texelFetch(g_screenTexture, screenPosI.xy, 0);        \n"
    "  vec4 pixel10 =                                                       \n"
    "      texelFetchOffset(g_screenTexture, screenPosI.xy, 0, ivec2(1, 0));\n"
    "  vec4 pixel20 =                                                       \n"
    "      texelFetchOffset(g_screenTexture, screenPosI.xy, 0, ivec2(2, 0));\n"
    "  vec4 pixel01 =                                                       \n"
    "      texelFetchOffset(g_screenTexture, screenPosI.xy, 0, ivec2(0, 1));\n"
    "  vec4 pixel11 =                                                       \n"
    "      texelFetchOffset(g_screenTexture, screenPosI.xy, 0, ivec2(1, 1));\n"
    "  vec4 pixel21 =                                                       \n"
    "      texelFetchOffset(g_screenTexture, screenPosI.xy, 0, ivec2(2, 1));\n"
    "  vec4 pixel02 =                                                       \n"
    "      texelFetchOffset(g_screenTexture, screenPosI.xy, 0, ivec2(0, 2));\n"
    "  vec4 pixel12 =                                                       \n"
    "      texelFetchOffset(g_screenTexture, screenPosI.xy, 0, ivec2(1, 2));\n"
    "                                                                       \n"
    "  float storeFlagPixel00 = 0.0;                                        \n"
    "  float storeFlagPixel10 = 0.0;                                        \n"
    "  float storeFlagPixel20 = 0.0;                                        \n"
    "  float storeFlagPixel01 = 0.0;                                        \n"
    "  float storeFlagPixel11 = 0.0;                                        \n"
    "  float storeFlagPixel21 = 0.0;                                        \n"
    "  float storeFlagPixel02 = 0.0;                                        \n"
    "  float storeFlagPixel12 = 0.0;                                        \n"
    "                                                                       \n"
    "  vec2 et;                                                             \n"
    "                                                                       \n"
    "#ifdef EDGE_DETECT_THRESHOLD                                           \n"
    "  float threshold = c_ColorThreshold;                                  \n"
    "#else                                                                  \n"
    "  float threshold = g_ColorThreshold;                                  \n"
    "#endif                                                                 \n"
    "                                                                       \n"
    "  {                                                                    \n"
    "    et.x = EdgeDetectColorCalcDiff(pixel00.rgb, pixel10.rgb);          \n"
    "    et.y = EdgeDetectColorCalcDiff(pixel00.rgb, pixel01.rgb);          \n"
    "    et = saturate(et - threshold);                                     \n"
    "    ivec2 eti = ivec2(et * 15.0 + 0.99);                               \n"
    "    outputEdges.x = uint(eti.x | (eti.y << 4));                        \n"
    "                                                                       \n"
    "    storeFlagPixel00 += et.x;                                          \n"
    "    storeFlagPixel00 += et.y;                                          \n"
    "    storeFlagPixel10 += et.x;                                          \n"
    "    storeFlagPixel01 += et.y;                                          \n"
    "  }                                                                    \n"
    "                                                                       \n"
    "  {                                                                    \n"
    "    et.x = EdgeDetectColorCalcDiff(pixel10.rgb, pixel20.rgb);          \n"
    "    et.y = EdgeDetectColorCalcDiff(pixel10.rgb, pixel11.rgb);          \n"
    "    et = saturate(et - threshold);                                     \n"
    "    ivec2 eti = ivec2(et * 15.0 + 0.99);                               \n"
    "    outputEdges.y = uint(eti.x | (eti.y << 4));                        \n"
    "                                                                       \n"
    "    storeFlagPixel10 += et.x;                                          \n"
    "    storeFlagPixel10 += et.y;                                          \n"
    "    storeFlagPixel20 += et.x;                                          \n"
    "    storeFlagPixel11 += et.y;                                          \n"
    "  }                                                                    \n"
    "                                                                       \n"
    "  {                                                                    \n"
    "    et.x = EdgeDetectColorCalcDiff(pixel01.rgb, pixel11.rgb);          \n"
    "    et.y = EdgeDetectColorCalcDiff(pixel01.rgb, pixel02.rgb);          \n"
    "    et = saturate(et - threshold);                                     \n"
    "    ivec2 eti = ivec2(et * 15.0 + 0.99);                               \n"
    "    outputEdges.z = uint(eti.x | (eti.y << 4));                        \n"
    "                                                                       \n"
    "    storeFlagPixel01 += et.x;                                          \n"
    "    storeFlagPixel01 += et.y;                                          \n"
    "    storeFlagPixel11 += et.x;                                          \n"
    "    storeFlagPixel02 += et.y;                                          \n"
    "  }                                                                    \n"
    "                                                                       \n"
    "  {                                                                    \n"
    "    et.x = EdgeDetectColorCalcDiff(pixel11.rgb, pixel21.rgb);          \n"
    "    et.y = EdgeDetectColorCalcDiff(pixel11.rgb, pixel12.rgb);          \n"
    "    et = saturate(et - threshold);                                     \n"
    "    ivec2 eti = ivec2(et * 15.0 + 0.99);                               \n"
    "    outputEdges.w = uint(eti.x | (eti.y << 4));                        \n"
    "                                                                       \n"
    "    storeFlagPixel11 += et.x;                                          \n"
    "    storeFlagPixel11 += et.y;                                          \n"
    "    storeFlagPixel21 += et.x;                                          \n"
    "    storeFlagPixel12 += et.y;                                          \n"
    "  }                                                                    \n"
    "                                                                       \n"
    "  gl_FragDepth = any(bvec4(outputEdges)) ? 1.0 : 0.0;                  \n"
    "                                                                       \n"
    "  if (gl_FragDepth != 0.0) {                                           \n"
    "    if (storeFlagPixel00 != 0.0)                                       \n"
    "      imageStore(g_resultTextureFlt4Slot1, screenPosI.xy + ivec2(0, 0),\n"
    "                 pixel00);                                             \n"
    "    if (storeFlagPixel10 != 0.0)                                       \n"
    "      imageStore(g_resultTextureFlt4Slot1, screenPosI.xy + ivec2(1, 0),\n"
    "                 pixel10);                                             \n"
    "    if (storeFlagPixel20 != 0.0)                                       \n"
    "      imageStore(g_resultTextureFlt4Slot1, screenPosI.xy + ivec2(2, 0),\n"
    "                 pixel20);                                             \n"
    "    if (storeFlagPixel01 != 0.0)                                       \n"
    "      imageStore(g_resultTextureFlt4Slot1, screenPosI.xy + ivec2(0, 1),\n"
    "                 pixel01);                                             \n"
    "    if (storeFlagPixel02 != 0.0)                                       \n"
    "      imageStore(g_resultTextureFlt4Slot1, screenPosI.xy + ivec2(0, 2),\n"
    "                 pixel02);                                             \n"
    "    if (storeFlagPixel11 != 0.0)                                       \n"
    "      imageStore(g_resultTextureFlt4Slot1, screenPosI.xy + ivec2(1, 1),\n"
    "                 pixel11);                                             \n"
    "    if (storeFlagPixel21 != 0.0)                                       \n"
    "      imageStore(g_resultTextureFlt4Slot1, screenPosI.xy + ivec2(2, 1),\n"
    "                 pixel21);                                             \n"
    "    if (storeFlagPixel12 != 0.0)                                       \n"
    "      imageStore(g_resultTextureFlt4Slot1, screenPosI.xy + ivec2(1, 2),\n"
    "                 pixel12);                                             \n"
    "  }                                                                    \n"
    "  outEdges = STORE_UVEC4(outputEdges);                                 \n"
    "}                                                                      \n"
    "#endif  // DETECT_EDGES1                                               \n"
    "                                                                       \n"
    "vec2 UnpackThresholds(uint val) {                                      \n"
    "  return vec2(val & 0x0Fu, val >> 4u) / 15.0f;                         \n"
    "}                                                                      \n"
    "                                                                       \n"
    "uint PruneNonDominantEdges(vec4 edges[3]) {                            \n"
    "  vec4 maxE4 = vec4(0.0, 0.0, 0.0, 0.0);                               \n"
    "                                                                       \n"
    "  float avg = 0.0;                                                     \n"
    "                                                                       \n"
    "  for (int i = 0; i < 3; i++) {                                        \n"
    "    maxE4 = max(maxE4, edges[i]);                                      \n"
    "                                                                       \n"
    "    avg = dot(edges[i], vec4(1, 1, 1, 1) / (3.0 * 4.0));               \n"
    "  }                                                                    \n"
    "                                                                       \n"
    "  vec2 maxE2 = max(maxE4.xy, maxE4.zw);                                \n"
    "  float maxE = max(maxE2.x, maxE2.y);                                  \n"
    "                                                                       \n"
    "  float threshold = avg * 0.65 + maxE * 0.35;                          \n"
    "                                                                       \n"
    "  // threshold = 0.0001; // this disables non-dominant edge pruning!   \n"
    "                                                                       \n"
    "  uint cx = edges[0].x >= threshold ? 1u : 0u;                         \n"
    "  uint cy = edges[0].y >= threshold ? 1u : 0u;                         \n"
    "  return PackEdge(uvec4(cx, cy, 0, 0));                                \n"
    "}                                                                      \n"
    "                                                                       \n"
    "void CollectEdges(int offX,                                            \n"
    "                  int offY,                                            \n"
    "                  out vec4 edges[3],                                   \n"
    "                  const uint packedVals[6 * 6]) {                      \n"
    "  vec2 pixelP0P0 = UnpackThresholds(packedVals[(offX)*6+(offY)]);      \n"
    "  vec2 pixelP1P0 = UnpackThresholds(packedVals[(offX+1)*6+(offY)]);    \n"
    "  vec2 pixelP0P1 = UnpackThresholds(packedVals[(offX)*6+(offY+1)]);    \n"
    "  vec2 pixelM1P0 = UnpackThresholds(packedVals[(offX-1)*6 +(offY)]);   \n"
    "  vec2 pixelP0M1 = UnpackThresholds(packedVals[(offX)*6+(offY-1)]);    \n"
    "  vec2 pixelP1M1 = UnpackThresholds(packedVals[(offX+1)*6 +(offY-1)]); \n"
    "  vec2 pixelM1P1 = UnpackThresholds(packedVals[(offX-1)*6+(offY+1)]);  \n"
    "                                                                       \n"
    "  edges[0].x = pixelP0P0.x;                                            \n"
    "  edges[0].y = pixelP0P0.y;                                            \n"
    "  edges[0].z = pixelP1P0.x;                                            \n"
    "  edges[0].w = pixelP1P0.y;                                            \n"
    "  edges[1].x = pixelP0P1.x;                                            \n"
    "  edges[1].y = pixelP0P1.y;                                            \n"
    "  edges[1].z = pixelM1P0.x;                                            \n"
    "  edges[1].w = pixelM1P0.y;                                            \n"
    "  edges[2].x = pixelP0M1.x;                                            \n"
    "  edges[2].y = pixelP0M1.y;                                            \n"
    "  edges[2].z = pixelP1M1.y;                                            \n"
    "  edges[2].w = pixelM1P1.x;                                            \n"
    "}                                                                      \n"
    "                                                                       \n"
    "#ifdef DETECT_EDGES2                                                   \n"
    "layout(early_fragment_tests) in;                                       \n"
    "void DetectEdges2() {                                                  \n"
    "  ivec2 screenPosI = ivec2(gl_FragCoord.xy);                           \n"
    "                                                                       \n"
    "  // source : edge differences from previous pass                      \n"
    "  uint packedVals[6 * 6];                                              \n"
    "                                                                       \n"
    "  // center pixel (our output)                                         \n"
    "  UVEC4 packedQ4 = texelFetch(g_src0Texture4Uint, screenPosI.xy, 0);   \n"
    "  packedVals[(2) * 6 + (2)] = LOAD_UINT(packedQ4.x);                   \n"
    "  packedVals[(3) * 6 + (2)] = LOAD_UINT(packedQ4.y);                   \n"
    "  packedVals[(2) * 6 + (3)] = LOAD_UINT(packedQ4.z);                   \n"
    "  packedVals[(3) * 6 + (3)] = LOAD_UINT(packedQ4.w);                   \n"
    "                                                                       \n"
    "  vec4 edges[3];                                                       \n"
    "  if (bool(packedVals[(2) * 6 + (2)]) ||                               \n"
    "      bool(packedVals[(3) * 6 + (2)])) {                               \n"
    "    UVEC4 packedQ1 = texelFetchOffset(g_src0Texture4Uint,              \n"
    "                                      screenPosI.xy, 0, ivec2(0, -1)); \n"
    "    packedVals[(2) * 6 + (0)] = LOAD_UINT(packedQ1.x);                 \n"
    "    packedVals[(3) * 6 + (0)] = LOAD_UINT(packedQ1.y);                 \n"
    "    packedVals[(2) * 6 + (1)] = LOAD_UINT(packedQ1.z);                 \n"
    "    packedVals[(3) * 6 + (1)] = LOAD_UINT(packedQ1.w);                 \n"
    "  }                                                                    \n"
    "                                                                       \n"
    "  if (bool(packedVals[(2) * 6 + (2)]) ||                               \n"
    "      bool(packedVals[(2) * 6 + (3)])) {                               \n"
    "    UVEC4 packedQ3 = texelFetchOffset(g_src0Texture4Uint,              \n"
    "                                      screenPosI.xy, 0, ivec2(-1, 0)); \n"
    "    packedVals[(0) * 6 + (2)] = LOAD_UINT(packedQ3.x);                 \n"
    "    packedVals[(1) * 6 + (2)] = LOAD_UINT(packedQ3.y);                 \n"
    "    packedVals[(0) * 6 + (3)] = LOAD_UINT(packedQ3.z);                 \n"
    "    packedVals[(1) * 6 + (3)] = LOAD_UINT(packedQ3.w);                 \n"
    "  }                                                                    \n"
    "                                                                       \n"
    "  if (bool(packedVals[(2) * 6 + (2)])) {                               \n"
    "    CollectEdges(2, 2, edges, packedVals);                             \n"
    "    uint pe = PruneNonDominantEdges(edges);                            \n"
    "    if (pe != 0u) {                                                    \n"
    "      imageStore(g_resultTexture, 2 * screenPosI.xy + ivec2(0, 0),     \n"
    "                 vec4(float(0x80u | pe) / 255.0, 0, 0, 0));            \n"
    "    }                                                                  \n"
    "  }                                                                    \n"
    "                                                                       \n"
    "  if (bool(packedVals[(3) * 6 + (2)]) ||                               \n"
    "      bool(packedVals[(3) * 6 + (3)])) {                               \n"
    "    UVEC4 packedQ5 = texelFetchOffset(g_src0Texture4Uint,              \n"
    "                                      screenPosI.xy, 0, ivec2(1, 0));  \n"
    "    packedVals[(4) * 6 + (2)] = LOAD_UINT(packedQ5.x);                 \n"
    "    packedVals[(5) * 6 + (2)] = LOAD_UINT(packedQ5.y);                 \n"
    "    packedVals[(4) * 6 + (3)] = LOAD_UINT(packedQ5.z);                 \n"
    "    packedVals[(5) * 6 + (3)] = LOAD_UINT(packedQ5.w);                 \n"
    "  }                                                                    \n"
    "                                                                       \n"
    "  if (bool(packedVals[(3) * 6 + (2)])) {                               \n"
    "    UVEC4 packedQ2 = texelFetchOffset(g_src0Texture4Uint,              \n"
    "                                      screenPosI.xy, 0, ivec2(1, -1)); \n"
    "    packedVals[(4) * 6 + (0)] = LOAD_UINT(packedQ2.x);                 \n"
    "    packedVals[(5) * 6 + (0)] = LOAD_UINT(packedQ2.y);                 \n"
    "    packedVals[(4) * 6 + (1)] = LOAD_UINT(packedQ2.z);                 \n"
    "    packedVals[(5) * 6 + (1)] = LOAD_UINT(packedQ2.w);                 \n"
    "                                                                       \n"
    "    CollectEdges(3, 2, edges, packedVals);                             \n"
    "    uint pe = PruneNonDominantEdges(edges);                            \n"
    "    if (pe != 0u) {                                                    \n"
    "      imageStore(g_resultTexture, 2 * screenPosI.xy + ivec2(1, 0),     \n"
    "                 vec4(float(0x80u | pe) / 255.0, 0, 0, 0));            \n"
    "    }                                                                  \n"
    "  }                                                                    \n"
    "                                                                       \n"
    "  if (bool(packedVals[(2) * 6 + (3)]) ||                               \n"
    "      bool(packedVals[(3) * 6 + (3)])) {                               \n"
    "    UVEC4 packedQ7 = texelFetchOffset(g_src0Texture4Uint,              \n"
    "                                      screenPosI.xy, 0, ivec2(0, 1));  \n"
    "    packedVals[(2) * 6 + (4)] = LOAD_UINT(packedQ7.x);                 \n"
    "    packedVals[(3) * 6 + (4)] = LOAD_UINT(packedQ7.y);                 \n"
    "    packedVals[(2) * 6 + (5)] = LOAD_UINT(packedQ7.z);                 \n"
    "    packedVals[(3) * 6 + (5)] = LOAD_UINT(packedQ7.w);                 \n"
    "  }                                                                    \n"
    "                                                                       \n"
    "  if (bool(packedVals[(2) * 6 + (3)])) {                               \n"
    "    UVEC4 packedQ6 = texelFetchOffset(g_src0Texture4Uint,              \n"
    "                                      screenPosI.xy, 0, ivec2(-1, -1));\n"
    "    packedVals[(0) * 6 + (4)] = LOAD_UINT(packedQ6.x);                 \n"
    "    packedVals[(1) * 6 + (4)] = LOAD_UINT(packedQ6.y);                 \n"
    "    packedVals[(0) * 6 + (5)] = LOAD_UINT(packedQ6.z);                 \n"
    "    packedVals[(1) * 6 + (5)] = LOAD_UINT(packedQ6.w);                 \n"
    "                                                                       \n"
    "    CollectEdges(2, 3, edges, packedVals);                             \n"
    "    uint pe = PruneNonDominantEdges(edges);                            \n"
    "    if (pe != 0u) {                                                    \n"
    "      imageStore(g_resultTexture, 2 * screenPosI.xy + ivec2(0, 1),     \n"
    "                 vec4(float(0x80u | pe) / 255.0, 0, 0, 0));            \n"
    "    }                                                                  \n"
    "  }                                                                    \n"
    "                                                                       \n"
    "  if (bool(packedVals[(3) * 6 + (3)])) {                               \n"
    "    CollectEdges(3, 3, edges, packedVals);                             \n"
    "    uint pe = PruneNonDominantEdges(edges);                            \n"
    "    if (pe != 0u) {                                                    \n"
    "      imageStore(g_resultTexture, 2 * screenPosI.xy + ivec2(1, 1),     \n"
    "                 vec4(float(0x80u | pe) / 255.0, 0, 0, 0));            \n"
    "    }                                                                  \n"
    "  }                                                                    \n"
    "}                                                                      \n"
    "#endif  // DETECT_EDGES2                                               \n"
    "                                                                       \n";

const char ApplyFramebufferAttachmentCMAAINTELResourceManager::cmaa_frag_s2_[] =
    "#ifdef COMBINE_EDGES                                                   \n"
    "void CombineEdges() {                                                  \n"
    "  ivec3 screenPosIBase = ivec3(ivec2(gl_FragCoord.xy) * 2, 0);         \n"
    "  vec3 screenPosBase = vec3(screenPosIBase);                           \n"
    "  uint packedEdgesArray[3 * 3];                                        \n"
    "                                                                       \n"
    "  // use only if it has the 'prev frame' flag:[sample * 255.0 - 127.5] \n"
    "  //-> if it has the last bit flag (128), it's going to stay above 0   \n"
    "  uvec4 sampA = uvec4(                                                 \n"
    "      textureGatherOffset(g_src0TextureFlt,                            \n"
    "                          screenPosBase.xy * g_OneOverScreenSize,      \n"
    "                          ivec2(1, 0)) * 255.0 - 127.5);               \n"
    "  uvec4 sampB = uvec4(                                                 \n"
    "      textureGatherOffset(g_src0TextureFlt,                            \n"
    "                          screenPosBase.xy * g_OneOverScreenSize,      \n"
    "                          ivec2(0, 1)) * 255.0 - 127.5);               \n"
    "  uint sampC = uint(                                                   \n"
    "      texelFetchOffset(g_src0TextureFlt, screenPosIBase.xy, 0,         \n"
    "                       ivec2(1, 1)).r * 255.0 - 127.5);                \n"
    "                                                                       \n"
    "  packedEdgesArray[(0) * 3 + (0)] = 0u;                                \n"
    "  packedEdgesArray[(1) * 3 + (0)] = sampA.w;                           \n"
    "  packedEdgesArray[(2) * 3 + (0)] = sampA.z;                           \n"
    "  packedEdgesArray[(1) * 3 + (1)] = sampA.x;                           \n"
    "  packedEdgesArray[(2) * 3 + (1)] = sampA.y;                           \n"
    "  packedEdgesArray[(0) * 3 + (1)] = sampB.w;                           \n"
    "  packedEdgesArray[(0) * 3 + (2)] = sampB.x;                           \n"
    "  packedEdgesArray[(1) * 3 + (2)] = sampB.y;                           \n"
    "  packedEdgesArray[(2) * 3 + (2)] = sampC;                             \n"
    "                                                                       \n"
    "  uvec4 pixelsC = uvec4(packedEdgesArray[(1 + 0) * 3 + (1 + 0)],       \n"
    "                        packedEdgesArray[(1 + 1) * 3 + (1 + 0)],       \n"
    "                        packedEdgesArray[(1 + 0) * 3 + (1 + 1)],       \n"
    "                        packedEdgesArray[(1 + 1) * 3 + (1 + 1)]);      \n"
    "  uvec4 pixelsL = uvec4(packedEdgesArray[(0 + 0) * 3 + (1 + 0)],       \n"
    "                        packedEdgesArray[(0 + 1) * 3 + (1 + 0)],       \n"
    "                        packedEdgesArray[(0 + 0) * 3 + (1 + 1)],       \n"
    "                        packedEdgesArray[(0 + 1) * 3 + (1 + 1)]);      \n"
    "  uvec4 pixelsU = uvec4(packedEdgesArray[(1 + 0) * 3 + (0 + 0)],       \n"
    "                        packedEdgesArray[(1 + 1) * 3 + (0 + 0)],       \n"
    "                        packedEdgesArray[(1 + 0) * 3 + (0 + 1)],       \n"
    "                        packedEdgesArray[(1 + 1) * 3 + (0 + 1)]);      \n"
    "                                                                       \n"
    "  uvec4 outEdge4 =                                                     \n"
    "      pixelsC | ((pixelsL & 0x01u) << 2u) | ((pixelsU & 0x02u) << 2u); \n"
    "  vec4 outEdge4Flt = vec4(outEdge4) / 255.0;                           \n"
    "                                                                       \n"
    "  imageStore(g_resultTextureSlot2, screenPosIBase.xy + ivec2(0, 0),    \n"
    "             outEdge4Flt.xxxx);                                        \n"
    "  imageStore(g_resultTextureSlot2, screenPosIBase.xy + ivec2(1, 0),    \n"
    "             outEdge4Flt.yyyy);                                        \n"
    "  imageStore(g_resultTextureSlot2, screenPosIBase.xy + ivec2(0, 1),    \n"
    "             outEdge4Flt.zzzz);                                        \n"
    "  imageStore(g_resultTextureSlot2, screenPosIBase.xy + ivec2(1, 1),    \n"
    "             outEdge4Flt.wwww);                                        \n"
    "                                                                       \n"
    "  // uvec4 numberOfEdges4 = uvec4(bitCount(outEdge4));                 \n"
    "  // gl_FragDepth =                                                    \n"
    "  //     any(greaterThan(numberOfEdges4, uvec4(1))) ? 1.0 : 0.0;       \n"
    "                                                                       \n"
    "  gl_FragDepth =                                                       \n"
    "      any(greaterThan(outEdge4, uvec4(1))) ? 1.0 : 0.0;                \n"
    "}                                                                      \n"
    "#endif  // COMBINE_EDGES                                               \n"
    "                                                                       \n"
    "#ifdef BLUR_EDGES                                                      \n"
    "layout(early_fragment_tests) in;                                       \n"
    "void BlurEdges() {                                                     \n"
    "  int _i;                                                              \n"
    "                                                                       \n"
    "  ivec3 screenPosIBase = ivec3(ivec2(gl_FragCoord.xy) * 2, 0);         \n"
    "  vec3 screenPosBase = vec3(screenPosIBase);                           \n"
    "  uint forFollowUpCount = 0u;                                          \n"
    "  ivec4 forFollowUpCoords[4];                                          \n"
    "                                                                       \n"
    "  uint packedEdgesArray[4 * 4];                                        \n"
    "                                                                       \n"
    "  uvec4 sampA = uvec4(                                                 \n"
    "      textureGatherOffset(g_src0TextureFlt,                            \n"
    "                          screenPosBase.xy * g_OneOverScreenSize,      \n"
    "                          ivec2(0, 0)) *255.5);                        \n"
    "  uvec4 sampB = uvec4(                                                 \n"
    "      textureGatherOffset(g_src0TextureFlt,                            \n"
    "                          screenPosBase.xy * g_OneOverScreenSize,      \n"
    "                          ivec2(2, 0)) *255.5);                        \n"
    "  uvec4 sampC = uvec4(                                                 \n"
    "      textureGatherOffset(g_src0TextureFlt,                            \n"
    "                          screenPosBase.xy * g_OneOverScreenSize,      \n"
    "                          ivec2(0, 2)) *255.5);                        \n"
    "  uvec4 sampD = uvec4(                                                 \n"
    "      textureGatherOffset(g_src0TextureFlt,                            \n"
    "                          screenPosBase.xy * g_OneOverScreenSize,      \n"
    "                          ivec2(2, 2)) *255.5);                        \n"
    "                                                                       \n"
    "  packedEdgesArray[(0) * 4 + (0)] = sampA.w;                           \n"
    "  packedEdgesArray[(1) * 4 + (0)] = sampA.z;                           \n"
    "  packedEdgesArray[(0) * 4 + (1)] = sampA.x;                           \n"
    "  packedEdgesArray[(1) * 4 + (1)] = sampA.y;                           \n"
    "  packedEdgesArray[(2) * 4 + (0)] = sampB.w;                           \n"
    "  packedEdgesArray[(3) * 4 + (0)] = sampB.z;                           \n"
    "  packedEdgesArray[(2) * 4 + (1)] = sampB.x;                           \n"
    "  packedEdgesArray[(3) * 4 + (1)] = sampB.y;                           \n"
    "  packedEdgesArray[(0) * 4 + (2)] = sampC.w;                           \n"
    "  packedEdgesArray[(1) * 4 + (2)] = sampC.z;                           \n"
    "  packedEdgesArray[(0) * 4 + (3)] = sampC.x;                           \n"
    "  packedEdgesArray[(1) * 4 + (3)] = sampC.y;                           \n"
    "  packedEdgesArray[(2) * 4 + (2)] = sampD.w;                           \n"
    "  packedEdgesArray[(3) * 4 + (2)] = sampD.z;                           \n"
    "  packedEdgesArray[(2) * 4 + (3)] = sampD.x;                           \n"
    "  packedEdgesArray[(3) * 4 + (3)] = sampD.y;                           \n"
    "                                                                       \n"
    "  for (_i = 0; _i < 4; _i++) {                                         \n"
    "    int _x = _i % 2;                                                   \n"
    "    int _y = _i / 2;                                                   \n"
    "                                                                       \n"
    "    ivec3 screenPosI = screenPosIBase + ivec3(_x, _y, 0);              \n"
    "                                                                       \n"
    "    uint packedEdgesC = packedEdgesArray[(1 + _x) * 4 + (1 + _y)];     \n"
    "                                                                       \n"
    "    uvec4 edges = UnpackEdge(packedEdgesC);                            \n"
    "    vec4 edgesFlt = vec4(edges);                                       \n"
    "                                                                       \n"
    "    float numberOfEdges = dot(edgesFlt, vec4(1, 1, 1, 1));             \n"
    "    if (numberOfEdges < 2.0)                                           \n"
    "      continue;                                                        \n"
    "                                                                       \n"
    "    float fromRight = edgesFlt.r;                                      \n"
    "    float fromBelow = edgesFlt.g;                                      \n"
    "    float fromLeft = edgesFlt.b;                                       \n"
    "    float fromAbove = edgesFlt.a;                                      \n"
    "                                                                       \n"
    "    vec4 xFroms = vec4(fromBelow, fromAbove, fromRight, fromLeft);     \n"
    "                                                                       \n"
    "    float blurCoeff = 0.0;                                             \n"
    "                                                                       \n"
    "    // These are additional blurs that complement the main line-based  \n"
    "    // blurring; Unlike line-based, these do not necessarily preserve  \n"
    "    // the total amount of screen colour as they will take             \n"
    "    // neighbouring pixel colours and apply them to the one currently  \n"
    "    // processed.                                                      \n"
    "                                                                       \n"
    "    // 1.) L-like shape.                                               \n"
    "    // For this shape, the total amount of screen colour will be       \n"
    "    // preserved when this is a part of a (zigzag) diagonal line as the\n"
    "    // corners from the other side  will do the same and take some of  \n"
    "    // the current pixel's colour in return.                           \n"
    "    // However, in the case when this is an actual corner, the pixel's \n"
    "    // colour will be partially overwritten by it's 2 neighbours.      \n"
    "    // if( numberOfEdges > 1.0 )                                       \n"
    "    {                                                                  \n"
    "      // with value of 0.15, the pixel will retain approx 77% of its   \n"
    "      // colour and the remaining 23% will come from its 2 neighbours  \n"
    "      // (which are likely to be blurred too in the opposite direction)\n"
    "      blurCoeff = 0.08;                                                \n"
    "                                                                       \n"
    "      // Only do blending if it's L shape - if we're between two       \n"
    "      // parallel edges, don't do anything                             \n"
    "      blurCoeff *= (1.0 - fromBelow * fromAbove) *                     \n"
    "                   (1.0 - fromRight * fromLeft);                       \n"
    "    }                                                                  \n"
    "                                                                       \n"
    "    // 2.) U-like shape (surrounded with edges from 3 sides)           \n"
    "    if (numberOfEdges > 2.0) {                                         \n"
    "      // with value of 0.13, the pixel will retain approx 72% of its   \n"
    "      // colour and the remaining 28% will be picked from its 3        \n"
    "      // neighbours (which are unlikely to be blurred too but could be)\n"
    "      blurCoeff = 0.11;                                                \n"
    "    }                                                                  \n"
    "                                                                       \n"
    "    // 3.) Completely surrounded with edges from all 4 sides           \n"
    "    if (numberOfEdges > 3.0) {                                         \n"
    "      // with value of 0.07, the pixel will retain 78% of its colour   \n"
    "      // and the remaining 22% will come from its 4 neighbours (which  \n"
    "      // are unlikely to be blurred)                                   \n"
    "      blurCoeff = 0.05;                                                \n"
    "    }                                                                  \n"
    "                                                                       \n"
    "    if (blurCoeff == 0.0) {                                            \n"
    "      // this avoids Z search below as well but that's ok because a Z  \n"
    "      // shape will also always have some blurCoeff                    \n"
    "      continue;                                                        \n"
    "    }                                                                  \n"
    "                                                                       \n"
    "    vec4 blurMap = xFroms * blurCoeff;                                 \n"
    "                                                                       \n"
    "    vec4 pixelC = texelFetch(g_screenTexture, screenPosI.xy, 0);       \n"
    "                                                                       \n"
    "    const float centerWeight = 1.0;                                    \n"
    "    float fromBelowWeight = blurMap.x;                                 \n"
    "    float fromAboveWeight = blurMap.y;                                 \n"
    "    float fromRightWeight = blurMap.z;                                 \n"
    "    float fromLeftWeight  = blurMap.w;                                 \n"
    "                                                                       \n"
    "    // this would be the proper math for blending if we were handling  \n"
    "    // lines (Zs) and mini kernel smoothing here, but since we're doing\n"
    "    // lines separately, no need to complicate, just tweak the settings\n"
    "    // float fromBelowWeight = (1.0 / (1.0 - blurMap.x)) - 1.0;        \n"
    "    // float fromAboveWeight = (1.0 / (1.0 - blurMap.y)) - 1.0;        \n"
    "    // float fromRightWeight = (1.0 / (1.0 - blurMap.z)) - 1.0;        \n"
    "    // float fromLeftWeight  = (1.0 / (1.0 - blurMap.w)) - 1.0;        \n"
    "                                                                       \n"
    "    float fourWeightSum = dot(blurMap, vec4(1, 1, 1, 1));              \n"
    "    float allWeightSum = centerWeight + fourWeightSum;                 \n"
    "                                                                       \n"
    "    vec4 color = vec4(0, 0, 0, 0);                                     \n"
    "    if (fromLeftWeight > 0.0) {                                        \n"
    "      vec3 pixelL = texelFetchOffset(g_screenTexture, screenPosI.xy, 0,\n"
    "                                     ivec2(-1, 0)).rgb;                \n"
    "      color.rgb += fromLeftWeight * pixelL;                            \n"
    "    }                                                                  \n"
    "    if (fromAboveWeight > 0.0) {                                       \n"
    "      vec3 pixelT = texelFetchOffset(g_screenTexture, screenPosI.xy, 0,\n"
    "                                     ivec2(0, -1)).rgb;                \n"
    "      color.rgb += fromAboveWeight * pixelT;                           \n"
    "    }                                                                  \n"
    "    if (fromRightWeight > 0.0) {                                       \n"
    "      vec3 pixelR = texelFetchOffset(g_screenTexture, screenPosI.xy, 0,\n"
    "                                     ivec2(1, 0)).rgb;                 \n"
    "      color.rgb += fromRightWeight * pixelR;                           \n"
    "    }                                                                  \n"
    "    if (fromBelowWeight > 0.0) {                                       \n"
    "      vec3 pixelB = texelFetchOffset(g_screenTexture, screenPosI.xy, 0,\n"
    "                                     ivec2(0, 1)).rgb;                 \n"
    "      color.rgb += fromBelowWeight * pixelB;                           \n"
    "    }                                                                  \n"
    "                                                                       \n"
    "    color /= fourWeightSum + 0.0001;                                   \n"
    "    color.a = 1.0 - centerWeight / allWeightSum;                       \n"
    "                                                                       \n"
    "    color.rgb = mix(pixelC.rgb, color.rgb, color.a).rgb;               \n"
    "#ifdef IN_GAMMA_CORRECT_MODE                                           \n"
    "    color.rgb = D3DX_FLOAT3_to_SRGB(color.rgb);                        \n"
    "#endif                                                                 \n"
    "                                                                       \n"
    "#ifdef DEBUG_OUTPUT_AAINFO                                             \n"
    "    imageStore(g_resultTextureSlot2, screenPosI.xy,                    \n"
    "               PackBlurAAInfo(screenPosI.xy, uint(numberOfEdges)));    \n"
    "#endif                                                                 \n"
    "    imageStore(g_resultTextureFlt4Slot1, screenPosI.xy,                \n"
    "               vec4(color.rgb, pixelC.a));                             \n"
    "                                                                       \n"
    "    if (numberOfEdges == 2.0) {                                        \n"
    "      uint packedEdgesL = packedEdgesArray[(0 + _x) * 4 + (1 + _y)];   \n"
    "      uint packedEdgesT = packedEdgesArray[(1 + _x) * 4 + (0 + _y)];   \n"
    "      uint packedEdgesR = packedEdgesArray[(2 + _x) * 4 + (1 + _y)];   \n"
    "      uint packedEdgesB = packedEdgesArray[(1 + _x) * 4 + (2 + _y)];   \n"
    "                                                                       \n"
    "      bool isHorizontalA = ((packedEdgesC) == (0x01u | 0x02u)) &&      \n"
    "         ((packedEdgesR & (0x01u | 0x08u)) == (0x08u));                \n"
    "      bool isHorizontalB = ((packedEdgesC) == (0x01u | 0x08u)) &&      \n"
    "         ((packedEdgesR & (0x01u | 0x02u)) == (0x02u));                \n"
    "                                                                       \n"
    "      bool isHCandidate = isHorizontalA || isHorizontalB;              \n"
    "                                                                       \n"
    "      bool isVerticalA = ((packedEdgesC) == (0x08u | 0x01u)) &&        \n"
    "         ((packedEdgesT & (0x08u | 0x04u)) == (0x04u));                \n"
    "      bool isVerticalB = ((packedEdgesC) == (0x08u | 0x04u)) &&        \n"
    "         ((packedEdgesT & (0x08u | 0x01u)) == (0x01u));                \n"
    "      bool isVCandidate = isVerticalA || isVerticalB;                  \n"
    "                                                                       \n"
    "      bool isCandidate = isHCandidate || isVCandidate;                 \n"
    "                                                                       \n"
    "      if (!isCandidate)                                                \n"
    "        continue;                                                      \n"
    "                                                                       \n"
    "      bool horizontal = isHCandidate;                                  \n"
    "                                                                       \n"
    "      // what if both are candidates? do additional pruning (still not \n"
    "      // 100% but gets rid of worst case errors)                       \n"
    "      if (isHCandidate && isVCandidate)                                \n"
    "        horizontal =                                                   \n"
    "           (isHorizontalA && ((packedEdgesL & 0x02u) == 0x02u)) ||     \n"
    "           (isHorizontalB && ((packedEdgesL & 0x08u) == 0x08u));       \n"
    "                                                                       \n"
    "      ivec2 offsetC;                                                   \n"
    "      uint packedEdgesM1P0;                                            \n"
    "      uint packedEdgesP1P0;                                            \n"
    "      if (horizontal) {                                                \n"
    "        packedEdgesM1P0 = packedEdgesL;                                \n"
    "        packedEdgesP1P0 = packedEdgesR;                                \n"
    "        offsetC = ivec2(2, 0);                                         \n"
    "      } else {                                                         \n"
    "        packedEdgesM1P0 = packedEdgesB;                                \n"
    "        packedEdgesP1P0 = packedEdgesT;                                \n"
    "        offsetC = ivec2(0, -2);                                        \n"
    "      }                                                                \n"
    "                                                                       \n"
    "      uvec4 edgesM1P0 = UnpackEdge(packedEdgesM1P0);                   \n"
    "      uvec4 edgesP1P0 = UnpackEdge(packedEdgesP1P0);                   \n"
    "      uvec4 edgesP2P0 = UnpackEdge(uint(texelFetch(                    \n"
    "         g_src0TextureFlt, screenPosI.xy + offsetC, 0).r * 255.5));    \n"
    "                                                                       \n"
    "      uvec4 arg0;                                                      \n"
    "      uvec4 arg1;                                                      \n"
    "      uvec4 arg2;                                                      \n"
    "      uvec4 arg3;                                                      \n"
    "      bool arg4;                                                       \n"
    "                                                                       \n"
    "      if (horizontal) {                                                \n"
    "        arg0 = uvec4(edges);                                           \n"
    "        arg1 = edgesM1P0;                                              \n"
    "        arg2 = edgesP1P0;                                              \n"
    "        arg3 = edgesP2P0;                                              \n"
    "        arg4 = true;                                                   \n"
    "      } else {                                                         \n"
    "        // Reuse the same code for vertical (used for horizontal above)\n"
    "        // but rotate input data 90º counter-clockwise, so that:       \n"
    "        // left     becomes     bottom                                 \n"
    "        // top      becomes     left                                   \n"
    "        // right    becomes     top                                    \n"
    "        // bottom   becomes     right                                  \n"
    "                                                                       \n"
    "        // we also have to rotate edges, thus .argb                    \n"
    "        arg0 = uvec4(edges.argb);                                      \n"
    "        arg1 = edgesM1P0.argb;                                         \n"
    "        arg2 = edgesP1P0.argb;                                         \n"
    "        arg3 = edgesP2P0.argb;                                         \n"
    "        arg4 = false;                                                  \n"
    "      }                                                                \n"
    "                                                                       \n"
    "      {                                                                \n"
    "        ivec2 screenPos = screenPosI.xy;                               \n"
    "        uvec4 _edges = arg0;                                           \n"
    "        uvec4 _edgesM1P0 = arg1;                                       \n"
    "        uvec4 _edgesP1P0 = arg2;                                       \n"
    "        uvec4 _edgesP2P0 = arg3;                                       \n"
    "        bool horizontal = arg4;                                        \n"
    "        // Inverted Z case:                                            \n"
    "        //   __                                                        \n"
    "        //  X|                                                         \n"
    "        // ¯¯                                                          \n"
    "        bool isInvertedZ = false;                                      \n"
    "        bool isNormalZ = false;                                        \n"
    "        {                                                              \n"
    "#ifndef SETTINGS_ALLOW_SHORT_Zs                                        \n"
    "          // (1u-_edges.a) constraint can be removed; it was added for \n"
    "          // some rare cases                                           \n"
    "          uint isZShape = _edges.r * _edges.g * _edgesM1P0.g *         \n"
    "             _edgesP1P0.a *_edgesP2P0.a * (1u - _edges.b) *            \n"
    "              (1u - _edgesP1P0.r) * (1u - _edges.a) *                  \n"
    "              (1u - _edgesP1P0.g);                                     \n"
    "#else                                                                  \n"
    "          uint isZShape = _edges.r * _edges.g * _edgesP1P0.a *         \n"
    "              (1u - _edges.b) * (1u - _edgesP1P0.r) * (1u - _edges.a) *\n"
    "                          (1u - _edgesP1P0.g);                         \n"
    "          isZShape *= (_edgesM1P0.g + _edgesP2P0.a);                   \n"
    "                          // and at least one of these need to be there\n"
    "#endif                                                                 \n"
    "          if (isZShape > 0u) {                                         \n"
    "            isInvertedZ = true;                                        \n"
    "          }                                                            \n"
    "        }                                                              \n"
    "                                                                       \n"
    "        // Normal Z case:                                              \n"
    "        // __                                                          \n"
    "        //  X|                                                         \n"
    "        //   ¯¯                                                        \n"
    "        {                                                              \n"
    "#ifndef SETTINGS_ALLOW_SHORT_Zs                                        \n"
    "          uint isZShape = _edges.r * _edges.a * _edgesM1P0.a *         \n"
    "              _edgesP1P0.g * _edgesP2P0.g * (1u - _edges.b) *          \n"
    "              (1u - _edgesP1P0.r) * (1u - _edges.g) *                  \n"
    "              (1u - _edgesP1P0.a);                                     \n"
    "#else                                                                  \n"
    "          uint isZShape = _edges.r * _edges.a * _edgesP1P0.g *         \n"
    "              (1u - _edges.b) * (1u - _edgesP1P0.r) * (1u - _edges.g) *\n"
    "              (1u - _edgesP1P0.a);                                     \n"
    "          isZShape *=                                                  \n"
    "              (_edgesM1P0.a + _edgesP2P0.g);                           \n"
    "                          // and at least one of these need to be there\n"
    "#endif                                                                 \n"
    "                                                                       \n"
    "          if (isZShape > 0u) {                                         \n"
    "            isNormalZ = true;                                          \n"
    "          }                                                            \n"
    "        }                                                              \n"
    "                                                                       \n"
    "        bool isZ = isInvertedZ || isNormalZ;                           \n"
    "        if (isZ) {                                                     \n"
    "          forFollowUpCoords[forFollowUpCount++] =                      \n"
    "              ivec4(screenPosI.xy, horizontal, isInvertedZ);           \n"
    "        }                                                              \n"
    "      }                                                                \n"
    "    }                                                                  \n"
    "  }                                                                    \n"
    "                                                                       \n"
    "  // This code below is the only potential bug with this algorithm :   \n"
    "  // it HAS to be executed after the simple shapes above. It used to be\n"
    "  // executed as separate compute shader (by storing the packed        \n"
    "  // 'forFollowUpCoords' in an append buffer and  consuming it later)  \n"
    "  // but the whole thing (append/consume buffers, using CS) appears to \n"
    "  // be too inefficient on most hardware.                              \n"
    "  // However, it seems to execute fairly efficiently here and without  \n"
    "  // any issues, although there is no 100% guarantee that this code    \n"
    "  // below will execute across all pixels (it has a c_maxLineLength    \n"
    "  // wide kernel) after other shaders processing same pixels have done \n"
    "  // solving simple shapes. It appears to work regardless, across all  \n"
    "  // hardware; pixels with 1-edge or two opposing edges are ignored by \n"
    "  // simple  shapes anyway and other shapes stop the long line         \n"
    "  // algorithm from executing the only danger appears to be simple     \n"
    "  // shape L's colliding with Z shapes from neighbouring pixels but I  \n"
    "  // couldn't reproduce any problems on any hardware.                  \n"
    "  for (uint _i = 0u; _i < forFollowUpCount; _i++) {                    \n"
    "    ivec4 data = forFollowUpCoords[_i];                                \n"
    "    ProcessDetectedZ(data.xy, bool(data.z), bool(data.w));             \n"
    "  }                                                                    \n"
    "}                                                                      \n"
    "#endif  // BLUR_EDGES                                                  \n"
    "                                                                       \n"
    "#ifdef DISPLAY_EDGES                                                   \n"
    "layout(location = 0) out vec4 color;                                   \n"
    "layout(location = 1) out vec4 hasEdges;                                \n"
    "void DisplayEdges() {                                                  \n"
    "  ivec2 screenPosI = ivec2(gl_FragCoord.xy);                           \n"
    "                                                                       \n"
    "  uint packedEdges, shapeType;                                         \n"
    "  UnpackBlurAAInfo(texelFetch(g_src0TextureFlt, screenPosI, 0).r,      \n"
    "                   packedEdges, shapeType);                            \n"
    "                                                                       \n"
    "  vec4 edges = vec4(UnpackEdge(packedEdges));                          \n"
    "  if (any(greaterThan(edges.xyzw, vec4(0)))) {                         \n"
    "#ifdef IN_BGR_MODE                                                     \n"
    "    color = c_edgeDebugColours[shapeType].bgra;                        \n"
    "#else                                                                  \n"
    "    color = c_edgeDebugColours[shapeType];                             \n"
    "#endif                                                                 \n"
    "    hasEdges = vec4(1.0);                                              \n"
    "  } else {                                                             \n"
    "    color = vec4(0);                                                   \n"
    "    hasEdges = vec4(0.0);                                              \n"
    "  }                                                                    \n"
    "}                                                                      \n"
    "#endif  // DISPLAY_EDGES                                               \n"
    "                                                                       \n"
    "void main() {                                                          \n"
    "#ifdef DETECT_EDGES1                                                   \n"
    "  DetectEdges1();                                                      \n"
    "#endif                                                                 \n"
    "#if defined DETECT_EDGES2                                              \n"
    "  DetectEdges2();                                                      \n"
    "#endif                                                                 \n"
    "#if defined COMBINE_EDGES                                              \n"
    "  CombineEdges();                                                      \n"
    "#endif                                                                 \n"
    "#if defined BLUR_EDGES                                                 \n"
    "  BlurEdges();                                                         \n"
    "#endif                                                                 \n"
    "#if defined DISPLAY_EDGES                                              \n"
    "  DisplayEdges();                                                      \n"
    "#endif                                                                 \n"
    "}                                                                      \n";

const char
    ApplyFramebufferAttachmentCMAAINTELResourceManager::copy_frag_str_[] =
        "precision highp float;                                             \n"
        "layout(binding = 0) uniform highp sampler2D inTexture;             \n"
        "layout(location = 0) out vec4 outColor;                            \n"
        "#ifdef GL_ES                                                       \n"
        "layout(binding = 0, rgba8) restrict writeonly uniform highp        \n"
        "                                               image2D outTexture; \n"
        "#else                                                              \n"
        "layout(rgba8) restrict writeonly uniform highp image2D outTexture; \n"
        "#endif                                                             \n"
        "                                                                   \n"
        "void main() {                                                      \n"
        "  ivec2 screenPosI = ivec2( gl_FragCoord.xy );                     \n"
        "  vec4 pixel = texelFetch(inTexture, screenPosI, 0);               \n"
        "#ifdef OUT_FBO                                                     \n"
        "  outColor = pixel;                                                \n"
        "#else                                                              \n"
        "  imageStore(outTexture, screenPosI, pixel);                       \n"
        "#endif                                                             \n"
        "}                                                                  \n";

}  // namespace gpu
