// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <math.h>
#include <stdio.h>
#include <string>

// Some tests for occlusion queries.
#include "gpu/demos/framework/demo_factory.h"
#include "gpu/demos/gles2_book/example.h"

namespace gpu {
namespace demos {

namespace {

class OcclusionQueryTest;

struct STUserData {
  OcclusionQueryTest* demo;
};

class OcclusionQueryTest : public gles2_book::Example<STUserData> {
 public:
  OcclusionQueryTest()
      : elasped_sec_(0),
        program_(0),
        matrix_loc_(0),
        color_loc_(0),
        vbo_(0),
        query_(0),
        clock_(0),
        last_query_status_(0) {
    test_ = this;
    RegisterCallbacks(stInit, stUpdate, stDraw, stShutDown);
  }

  const wchar_t* Title() const {
    return L"Occlusion Query Test";
  }

  bool IsAnimated() {
    return true;
  }

 private:
  int Init(ESContext* esContext);
  void Update(ESContext* esContext, float elapsed_sec);
  void Draw(ESContext* esContext);
  void ShutDown(ESContext* esContext);

  void InitShaders();
  void DrawRect(float x, float z, float scale, float* color);

  static int stInit(ESContext* esContext) {
    static_cast<STUserData*>(esContext->userData)->demo = test_;
    test_ = NULL;
    return static_cast<STUserData*>(esContext->userData)->demo->Init(esContext);
  }

  static void stUpdate (ESContext* esContext, float elapsed_sec) {
    static_cast<STUserData*>(esContext->userData)->demo->Update(
       esContext, elapsed_sec);
  }

  static void stShutDown (ESContext* esContext) {
    static_cast<STUserData*>(esContext->userData)->demo->ShutDown(
       esContext);
  }

  static void stDraw (ESContext* esContext) {
    static_cast<STUserData*>(esContext->userData)->demo->Draw(
       esContext);
  }

  // This is the test being created. Because the GLES2 book's framework
  // has no way to pass anything into the init funciton this is a hacky
  // workaround.
  static OcclusionQueryTest* test_;

  float elasped_sec_;
  GLuint program_;
  GLint matrix_loc_;
  GLint color_loc_;
  GLuint vbo_;
  GLuint query_;
  float clock_;
  GLuint last_query_status_;

  static float red_[4];
  static float green_[4];
  static float blue_[4];
};

OcclusionQueryTest* OcclusionQueryTest::test_;
float OcclusionQueryTest::red_[4] = { 1, 0, 0, 1, };
float OcclusionQueryTest::green_[4] = { 0, 1, 0, 1, };
float OcclusionQueryTest::blue_[4] = { 0, 0, 1, 1, };

void CheckGLError(const char* func_name, int line_no) {
#ifndef NDEBUG
  GLenum error = GL_NO_ERROR;
  while ((error = glGetError()) != GL_NO_ERROR) {
    fprintf(stderr, "GL ERROR in %s at line %d : 0x%04x\n",
            func_name, line_no, error);
  }
#endif
}

GLuint LoadShader(GLenum type, const char* shaderSrc) {
  CheckGLError("LoadShader", __LINE__);
  GLuint shader = glCreateShader(type);
  if (shader == 0) {
    return 0;
  }
  // Load the shader source
  glShaderSource(shader, 1, &shaderSrc, NULL);
  // Compile the shader
  glCompileShader(shader);
  // Check the compile status
  GLint value = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &value);
  if (value == 0) {
    char buffer[1024];
    GLsizei length = 0;
    glGetShaderInfoLog(shader, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    fprintf(stderr, "Error compiling shader: %s\n", log.c_str());
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

void OcclusionQueryTest::InitShaders() {
  static const char* v_shader_str =
    "uniform mat4 worldMatrix;\n"
    "attribute vec3 g_Position;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = worldMatrix *\n"
    "                 vec4(g_Position.x, g_Position.y, g_Position.z, 1.0);\n"
    "}\n";
  static const char* f_shader_str =
    "precision mediump float;"
    "uniform vec4 color;\n"
    "void main()\n"
    "{\n"
    "  gl_FragColor = color;\n"
    "}\n";

  CheckGLError("InitShaders", __LINE__);
  GLuint vertex_shader = LoadShader(GL_VERTEX_SHADER, v_shader_str);
  GLuint fragment_shader = LoadShader(GL_FRAGMENT_SHADER, f_shader_str);
  // Create the program object
  GLuint program = glCreateProgram();
  if (program == 0) {
    fprintf(stderr, "Creating program failed\n");
    return;
  }
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  // Bind g_Position to attribute 0
  glBindAttribLocation(program, 0, "g_Position");
  // Link the program
  glLinkProgram(program);
  // Check the link status
  GLint linked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (linked == 0) {
    char buffer[1024];
    GLsizei length = 0;
    glGetProgramInfoLog(program, sizeof(buffer), &length, buffer);
    std::string log(buffer, length);
    fprintf(stderr, "Error linking program: %s\n", log.c_str());
    glDeleteProgram(program);
    return;
  }
  program_ = program;
  matrix_loc_ = glGetUniformLocation(program_, "worldMatrix");
  color_loc_ = glGetUniformLocation(program_, "color");
  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  static float vertices[] = {
      1,  1, 0.0,
     -1,  1, 0.0,
     -1, -1, 0.0,
      1,  1, 0.0,
     -1, -1, 0.0,
      1, -1, 0.0,
  };
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(vertices),
               NULL,
               GL_STATIC_DRAW);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
  CheckGLError("InitShaders", __LINE__);

  glGenQueriesEXT(1, &query_);
  glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, query_);
  glEndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);
  CheckGLError("InitShaders", __LINE__);
}

}  // anonymous namespace.

int OcclusionQueryTest::Init(ESContext* esContext) {
  CheckGLError("GLFromCPPInit", __LINE__);
  glClearColor(0.0f, 0.1f, 0.2f, 1.0f);
  InitShaders();
  CheckGLError("GLFromCPPInit", __LINE__);
  return 1;
}

void OcclusionQueryTest::Update(ESContext* esContext, float elapsed_sec) {
  elasped_sec_ = elapsed_sec;
}

static void SetMatrix(float x, float z, float scale, float* matrix) {
  matrix[0] = scale;
  matrix[1] = 0.0f;
  matrix[2] = 0.0f;
  matrix[3] = 0.0f;

  matrix[4] = 0.0f;
  matrix[5] = scale;
  matrix[6] = 0.0f;
  matrix[7] = 0.0f;

  matrix[8] = 0.0f;
  matrix[9] = 0.0f;
  matrix[10] = scale;
  matrix[11] = 0.0f;

  matrix[12] = x;
  matrix[13] = 0.0f;
  matrix[14] = z;
  matrix[15] = 1.0f;
}

void OcclusionQueryTest::DrawRect(float x, float z, float scale, float* color) {
  GLfloat matrix[16];

  SetMatrix(x, z, scale, matrix);

  // Set up the model matrix
  glUniformMatrix4fv(matrix_loc_, 1, GL_FALSE, matrix);

  glUniform4fv(color_loc_, 1, color);
  CheckGLError("GLFromCPPDraw", __LINE__);

  glDrawArrays(GL_TRIANGLES, 0, 6);
  CheckGLError("GLFromCPPDraw", __LINE__);
}

void OcclusionQueryTest::Draw(ESContext* esContext) {
  CheckGLError("GLFromCPPDraw", __LINE__);
  clock_ += elasped_sec_;

  // Note: the viewport is automatically set up to cover the entire Canvas.
  // Clear the color buffer
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  CheckGLError("GLFromCPPDraw", __LINE__);
  // Use the program object
  glUseProgram(program_);
  CheckGLError("GLFromCPPDraw", __LINE__);

  // Load the vertex data
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
  CheckGLError("GLFromCPPDraw", __LINE__);

  DrawRect(sinf(clock_), 0.0f, 0.50f,
           last_query_status_ ? green_ : red_);

  bool started_query = false;
  GLuint result = 0;
  glGetQueryObjectuivEXT(query_, GL_QUERY_RESULT_AVAILABLE_EXT, &result);
  CheckGLError("GLFromCPPDraw", __LINE__);
  if (result) {
    glGetQueryObjectuivEXT(query_, GL_QUERY_RESULT_EXT, &last_query_status_);
    CheckGLError("GLFromCPPDraw", __LINE__);
    glBeginQueryEXT(GL_ANY_SAMPLES_PASSED_EXT, query_);
    CheckGLError("GLFromCPPDraw", __LINE__);
    started_query = true;
  }

  DrawRect(-0.125f, 0.1f, 0.25f, blue_);

  if (started_query) {
    glEndQueryEXT(GL_ANY_SAMPLES_PASSED_EXT);
    CheckGLError("GLFromCPPDraw", __LINE__);
  }

  glFlush();
}

void OcclusionQueryTest::ShutDown(ESContext* esContext) {
}

Demo* CreateDemo() {
  return new OcclusionQueryTest();
}

}  // namespace demos
}  // namespace gpu


