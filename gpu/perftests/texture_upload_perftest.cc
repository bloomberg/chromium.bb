// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/containers/small_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
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
    v_texCoord = vec2((a_position.x + 1) * 0.5, (a_position.y + 1) * 0.5);
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
    surface_ = gfx::GLSurface::CreateOffscreenGLSurface(size_);
    gl_context_ = gfx::GLContext::CreateGLContext(NULL,  // share_group
                                                  surface_.get(),
                                                  gfx::PreferIntegratedGpu);

    // Prepare a simple program and a vertex buffer that will be
    // used to draw a quad on the offscreen surface.
    ui::ScopedMakeCurrent smc(gl_context_.get(), surface_.get());
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
    if (program_object_ != 0) {
      glDeleteProgram(program_object_);
    }
    if (vertex_shader_ != 0) {
      glDeleteShader(vertex_shader_);
    }
    if (fragment_shader_ != 0) {
      glDeleteShader(fragment_shader_);
    }
    if (vertex_buffer_ != 0) {
      glDeleteShader(vertex_buffer_);
    }

    gl_context_ = nullptr;
    surface_ = nullptr;
  }

 protected:
  struct Measurement {
    Measurement() : name(), wall_time(){};
    Measurement(const std::string& name, const base::TimeDelta wall_time)
        : name(name), wall_time(wall_time){};
    std::string name;
    base::TimeDelta wall_time;
  };
  // Upload and draw on the offscren surface.
  // Return a list of pair. Each pair describe a gl operation and the wall
  // time elapsed in milliseconds.
  std::vector<Measurement> UploadAndDraw(const std::vector<uint8>& pixels,
                                         const GLenum format,
                                         const GLenum type) {
    std::vector<Measurement> measurements;
    ui::ScopedMakeCurrent smc(gl_context_.get(), surface_.get());

    base::ElapsedTimer total_timer;
    GLuint texture_id = 0;

    base::ElapsedTimer tex_timer;
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexImage2D(GL_TEXTURE_2D, 0, format, size_.width(), size_.height(), 0,
                 format, type, &pixels[0]);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    CheckNoGlError();
    measurements.push_back(Measurement("teximage2d", tex_timer.Elapsed()));

    base::ElapsedTimer draw_timer;
    glUseProgram(program_object_);
    glUniform1i(sampler_location_, 0);

    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    measurements.push_back(Measurement("drawarrays", draw_timer.Elapsed()));

    base::ElapsedTimer finish_timer;
    glFinish();
    CheckNoGlError();
    measurements.push_back(Measurement("finish", finish_timer.Elapsed()));
    measurements.push_back(Measurement("total", total_timer.Elapsed()));

    glDeleteTextures(1, &texture_id);

    std::vector<uint8> pixels_rendered(size_.GetArea() * 4);
    glReadPixels(0, 0, size_.width(), size_.height(), GL_RGBA, type,
                 &pixels_rendered[0]);
    CheckNoGlError();

    // TODO(dcastagna): don't assume the format of the texture and do
    // the appropriate format conversion.
    EXPECT_EQ(static_cast<GLenum>(GL_RGBA), format);
    EXPECT_EQ(pixels, pixels_rendered);
    return measurements;
  }

  const gfx::Size size_;  // for the offscreen surface and the texture
  scoped_refptr<gfx::GLContext> gl_context_;
  scoped_refptr<gfx::GLSurface> surface_;

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
        agg.wall_time += m.wall_time;
      }
    }
  }

  for (const auto& entry : aggregates) {
    const auto& m = entry.second;
    perf_test::PrintResult(
        m.name, "", "", (m.wall_time / kUploadPerfTestRuns).InMillisecondsF(),
        "ms", true);
  }
}

}  // namespace
}  // namespace gpu
