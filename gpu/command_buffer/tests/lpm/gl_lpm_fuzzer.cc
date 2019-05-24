// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>
#include <stdint.h>

#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/strings/string_split.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "gpu/command_buffer/tests/lpm/gl_lpm_fuzzer.pb.h"
#include "gpu/command_buffer/tests/lpm/gl_lpm_shader_to_string.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "ui/gfx/extension_set.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"

struct Env {
  Env() {
    CHECK(base::i18n::InitializeICU());
    base::CommandLine::Init(0, nullptr);
    auto* command_line = base::CommandLine::ForCurrentProcess();

    // TODO(nedwill): support switches for swiftshader, etc.
    command_line->AppendSwitchASCII(switches::kUseGL,
                                    gl::kGLImplementationANGLEName);
    command_line->AppendSwitchASCII(switches::kUseANGLE,
                                    gl::kANGLEImplementationNullName);
    base::FeatureList::InitializeInstance(std::string(), std::string());
    base::MessageLoopForIO message_loop;
    gpu::GLTestHelper::InitializeGLDefault();
    ::gles2::Initialize();
  }

  base::AtExitManager at_exit;
};

class ScopedGLManager {
 public:
  ScopedGLManager() {
    gpu::GLManager::Options options;
    gl_.Initialize(options);
  }
  ~ScopedGLManager() { gl_.Destroy(); }

 private:
  gpu::GLManager gl_;
};

GLuint CompileShader(GLenum type, const char* shaderSrc) {
  GLuint shader = glCreateShader(type);
  // Load the shader source
  glShaderSource(shader, 1, &shaderSrc, nullptr);
  // Compile the shader
  glCompileShader(shader);

  return shader;
}

const char* acceptable_errors[] = {
    "void function cannot return a value",
    "function already has a body",
    "undeclared identifier",
    "l-value required (can't modify a const)",
    "cannot convert from",
    "main function cannot return a value",
    "illegal use of type 'void'",
    "boolean expression expected",
    "Missing main()",
    "Divide by zero error during constant folding",
    // TODO(nedwill): enable GLSL ES 3.00
    "operator supported in GLSL ES 3.00 and above only",
    "wrong operand types",
    "function must have the same return type in all of its declarations",
    "function return is not matching type",
    "redefinition",
    "WARNING:",
    "can't modify void",
};

// Filter errors which we don't think interfere with fuzzing everything.
bool ErrorOk(const base::StringPiece line) {
  for (base::StringPiece acceptable_error : acceptable_errors) {
    if (line.find(acceptable_error) != base::StringPiece::npos) {
      return true;
    }
  }
  LOG(WARNING) << "failed due to line: " << line;
  return false;
}

bool ErrorsOk(const base::StringPiece log) {
  std::vector<std::string> lines = base::SplitString(
      log, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& line : lines) {
    if (!ErrorOk(line)) {
      return false;
    }
  }
  return true;
}

GLuint LoadShader(GLenum type, const fuzzing::Shader& shader_proto) {
  std::string shader_s = gl_lpm_fuzzer::GetShader(shader_proto);
  if (shader_s.empty()) {
    return 0;
  }

  GLuint shader = CompileShader(type, shader_s.c_str());

  // Check the compile status
  GLint value = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &value);
  if (value == 0) {
    char buffer[1024];
    GLsizei length = 0;
    glGetShaderInfoLog(shader, sizeof(buffer), &length, buffer);
    base::StringPiece log(buffer, length);
    if (value != 1 && !ErrorsOk(log)) {
      LOG(WARNING) << "Encountered an unexpected failure when translating:\n"
                   << log << "\nfailed to compile shader:\n"
                   << shader_proto.DebugString() << "converted:\n"
                   << shader_s;
    }
    glDeleteShader(shader);
    shader = 0;
  }
  return shader;
}

DEFINE_PROTO_FUZZER(const fuzzing::Session& session) {
  static Env* env = new Env();
  CHECK(env);
  // TODO(nedwill): Creating a new GLManager on each iteration
  // is expensive. We should investigate ways to avoid expensive
  // initialization.
  ScopedGLManager scoped_gl_manager;

  GLuint vertex_shader_id =
      LoadShader(GL_VERTEX_SHADER, session.vertex_shader());
  GLuint fragment_shader_id =
      LoadShader(GL_FRAGMENT_SHADER, session.fragment_shader());
  if (!vertex_shader_id || !fragment_shader_id) {
    return;
  }

  GLuint program =
      gpu::GLTestHelper::SetupProgram(vertex_shader_id, fragment_shader_id);
  if (!program) {
    return;
  }

  glUseProgram(program);
  // Relink program.
  glLinkProgram(program);
}
