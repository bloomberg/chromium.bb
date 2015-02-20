// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/containers/small_map.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/service/gpu_timing.h"
#include "gpu/perftests/measurements.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {
namespace {

const int kUploadPerfWarmupRuns = 10;
const int kUploadPerfTestRuns = 100;

#define SHADER(Src) #Src

// clang-format off
const char kVertexShader[] =
SHADER(
  attribute vec2 a_position;
  varying vec2 v_texCoord;
  void main() {
    gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);
    v_texCoord = vec2((a_position.x + 1.0) * 0.5, (a_position.y + 1.0) * 0.5);
  }
);
const char kFragmentShader[] =
SHADER(
  uniform sampler2D a_texture;
  varying vec2 v_texCoord;
  void main() {
    gl_FragColor = texture2D(a_texture, v_texCoord.xy);
  }
);
// clang-format on

void CheckNoGlError() {
  CHECK_EQ(static_cast<GLenum>(GL_NO_ERROR), glGetError());
}

// Utility function to compile a shader from a string.
GLuint LoadShader(const GLenum type, const char* const src) {
  GLuint shader = 0;
  shader = glCreateShader(type);
  CHECK_NE(0u, shader);
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);

  GLint compiled = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled == 0) {
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    if (len > 1) {
      scoped_ptr<char> error_log(new char[len]);
      glGetShaderInfoLog(shader, len, NULL, error_log.get());
      LOG(ERROR) << "Error compiling shader: " << error_log.get();
    }
  }
  CHECK_NE(0, compiled);
  return shader;
}

void GenerateTextureData(const gfx::Size& size,
                         const int seed,
                         std::vector<uint8>* const pixels) {
  pixels->resize(size.GetArea() * 4);
  for (int y = 0; y < size.height(); ++y) {
    for (int x = 0; x < size.width(); ++x) {
      const size_t offset = (y * size.width() + x) * 4;
      pixels->at(offset) = (y + seed) % 256;
      pixels->at(offset + 1) = (x + seed) % 256;
      pixels->at(offset + 2) = (y + x + seed) % 256;
      pixels->at(offset + 3) = 255;
    }
  }
}

// PerfTest to check costs of texture upload at different stages
// on different platforms.
class TextureUploadPerfTest : public testing::Test {
 public:
  TextureUploadPerfTest() : size_(512, 512) {}

  // Overridden from testing::Test
  void SetUp() override {
    // Initialize an offscreen surface and a gl context.
    gfx::GLSurface::InitializeOneOff();
    surface_ = gfx::GLSurface::CreateOffscreenGLSurface(gfx::Size(4, 4));
    gl_context_ = gfx::GLContext::CreateGLContext(NULL,  // share_group
                                                  surface_.get(),
                                                  gfx::PreferIntegratedGpu);
    ui::ScopedMakeCurrent smc(gl_context_.get(), surface_.get());
    glGenTextures(1, &color_texture_);
    glBindTexture(GL_TEXTURE_2D, color_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_.width(), size_.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glGenFramebuffersEXT(1, &framebuffer_object_);
    glBindFramebufferEXT(GL_FRAMEBUFFER, framebuffer_object_);

    glFramebufferTexture2DEXT(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, color_texture_, 0);
    DCHECK_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
              glCheckFramebufferStatusEXT(GL_FRAMEBUFFER));

    glViewport(0, 0, size_.width(), size_.height());

    if (gpu_timing_.Initialize(gl_context_.get())) {
      LOG(INFO) << "Gpu timing initialized with timer type: "
                << gpu_timing_.GetTimerTypeName();
      gpu_timing_.CheckAndResetTimerErrors();
      gpu_timing_.InvalidateTimerOffset();
    } else {
      LOG(WARNING) << "Can't initialize gpu timing";
    }
    // Prepare a simple program and a vertex buffer that will be
    // used to draw a quad on the offscreen surface.
    vertex_shader_ = LoadShader(GL_VERTEX_SHADER, kVertexShader);
    fragment_shader_ = LoadShader(GL_FRAGMENT_SHADER, kFragmentShader);
    program_object_ = glCreateProgram();
    CHECK_NE(0u, program_object_);

    glAttachShader(program_object_, vertex_shader_);
    glAttachShader(program_object_, fragment_shader_);
    glBindAttribLocation(program_object_, 0, "a_position");
    glLinkProgram(program_object_);

    GLint linked = -1;
    glGetProgramiv(program_object_, GL_LINK_STATUS, &linked);
    CHECK_NE(0, linked);

    sampler_location_ = glGetUniformLocation(program_object_, "a_texture");
    CHECK_NE(-1, sampler_location_);

    glGenBuffersARB(1, &vertex_buffer_);
    CHECK_NE(0u, vertex_buffer_);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    static GLfloat positions[] = {
        -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
    CheckNoGlError();
  }

  void TearDown() override {
    ui::ScopedMakeCurrent smc(gl_context_.get(), surface_.get());
    glDeleteProgram(program_object_);
    glDeleteShader(vertex_shader_);
    glDeleteShader(fragment_shader_);
    glDeleteShader(vertex_buffer_);

    glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffersEXT(1, &framebuffer_object_);
    glDeleteTextures(1, &color_texture_);

    gl_context_ = nullptr;
    surface_ = nullptr;
  }

 protected:
  // Upload and draw on the offscren surface.
  // Return a list of pair. Each pair describe a gl operation and the wall
  // time elapsed in milliseconds.
  std::vector<Measurement> UploadAndDraw(const std::vector<uint8>& pixels,
                                         const GLenum format,
                                         const GLenum type) {
    ui::ScopedMakeCurrent smc(gl_context_.get(), surface_.get());
    DCHECK_NE(0u, framebuffer_object_);
    glBindFramebufferEXT(GL_FRAMEBUFFER, framebuffer_object_);

    MeasurementTimers total_timers(&gpu_timing_);
    GLuint texture_id = 0;

    MeasurementTimers tex_timers(&gpu_timing_);
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexImage2D(GL_TEXTURE_2D, 0, format, size_.width(), size_.height(), 0,
                 format, type, &pixels[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    CheckNoGlError();
    tex_timers.Record();

    MeasurementTimers draw_timers(&gpu_timing_);
    glUseProgram(program_object_);
    glUniform1i(sampler_location_, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    draw_timers.Record();

    MeasurementTimers finish_timers(&gpu_timing_);
    glFinish();
    CheckNoGlError();
    finish_timers.Record();
    total_timers.Record();

    glDeleteTextures(1, &texture_id);

    std::vector<uint8> pixels_rendered(size_.GetArea() * 4);
    glReadPixels(0, 0, size_.width(), size_.height(), GL_RGBA, type,
                 &pixels_rendered[0]);
    CheckNoGlError();

    // TODO(dcastagna): don't assume the format of the texture and do
    // the appropriate format conversion.
    EXPECT_EQ(static_cast<GLenum>(GL_RGBA), format);
    EXPECT_EQ(pixels, pixels_rendered);

    std::vector<Measurement> measurements;
    measurements.push_back(total_timers.GetAsMeasurement("total"));
    measurements.push_back(tex_timers.GetAsMeasurement("teximage2d"));
    measurements.push_back(draw_timers.GetAsMeasurement("drawarrays"));
    measurements.push_back(finish_timers.GetAsMeasurement("finish"));
    return measurements;
  }

  const gfx::Size size_;  // for the fbo and the texture
  scoped_refptr<gfx::GLContext> gl_context_;
  scoped_refptr<gfx::GLSurface> surface_;
  GPUTiming gpu_timing_;

  GLuint color_texture_ = 0;
  GLuint framebuffer_object_ = 0;
  GLuint vertex_shader_ = 0;
  GLuint fragment_shader_ = 0;
  GLuint program_object_ = 0;
  GLint sampler_location_ = -1;
  GLuint vertex_buffer_ = 0;
};

// Perf test that generates, uploads and draws a texture on a surface repeatedly
// and prints out aggregated measurements for all the runs.
TEST_F(TextureUploadPerfTest, glTexImage2d) {
  std::vector<uint8> pixels;
  base::SmallMap<std::map<std::string, Measurement>>
      aggregates;  // indexed by name
  for (int i = 0; i < kUploadPerfWarmupRuns + kUploadPerfTestRuns; ++i) {
    GenerateTextureData(size_, i + 1, &pixels);
    auto run = UploadAndDraw(pixels, GL_RGBA, GL_UNSIGNED_BYTE);
    if (i >= kUploadPerfWarmupRuns) {
      for (const Measurement& m : run) {
        auto& agg = aggregates[m.name];
        agg.name = m.name;
        agg.Increment(m);
      }
    }
  }

  for (const auto& entry : aggregates) {
    const auto m = entry.second.Divide(kUploadPerfTestRuns);
    m.PrintResult();
  }
}

}  // namespace
}  // namespace gpu
