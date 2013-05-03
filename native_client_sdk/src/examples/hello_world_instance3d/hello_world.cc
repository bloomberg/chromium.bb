/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/** @file hello_world_gles.cc
 * This example demonstrates loading and running a simple 3D openGL ES 2.0
 * application.
 */

//---------------------------------------------------------------------------
// The spinning Cube
//---------------------------------------------------------------------------

#define _USE_MATH_DEFINES 1
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nacl_io/nacl_io.h"

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_graphics_3d.h"
#include "ppapi/c/ppp_instance.h"

#include "ppapi/lib/gl/gles2/gl2ext_ppapi.h"

#include "ppapi_main/ppapi_event.h"
#include "ppapi_main/ppapi_instance3d.h"
#include "ppapi_main/ppapi_main.h"

#include <GLES2/gl2.h>
#include "matrix.h"

GLuint g_positionLoc;
GLuint g_texCoordLoc;
GLuint g_colorLoc;
GLuint g_MVPLoc;
GLuint g_vboID;
GLuint g_ibID;
GLubyte g_Indices[36];

GLuint g_programObj;
GLuint g_vertexShader;
GLuint g_fragmentShader;

GLuint g_textureLoc = 0;
GLuint g_textureID = 0;

float g_fSpinX = 0.0f;
float g_fSpinY = 0.0f;

//----------------------------------------------------------------------------
// Rendering Assets
//----------------------------------------------------------------------------
struct Vertex {
  float tu, tv;
  float color[3];
  float loc[3];
};

Vertex* g_quadVertices = NULL;
const char* g_TextureData = NULL;
const char* g_VShaderData = NULL;
const char* g_FShaderData = NULL;

bool g_Loaded = false;
bool g_Ready = false;

float g_xSpin = 2.0f;
float g_ySpin = 0.5f;

GLuint compileShader(GLenum type, const char* data) {
  const char* shaderStrings[1];
  shaderStrings[0] = data;

  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, shaderStrings, NULL);
  glCompileShader(shader);
  return shader;
}

void InitProgram(void) {
  g_vertexShader = compileShader(GL_VERTEX_SHADER, g_VShaderData);
  g_fragmentShader = compileShader(GL_FRAGMENT_SHADER, g_FShaderData);

  g_programObj = glCreateProgram();
  glAttachShader(g_programObj, g_vertexShader);
  glAttachShader(g_programObj, g_fragmentShader);
  glLinkProgram(g_programObj);

  glGenBuffers(1, &g_vboID);
  glBindBuffer(GL_ARRAY_BUFFER, g_vboID);
  glBufferData(GL_ARRAY_BUFFER,
               24 * sizeof(Vertex),
               (void*)&g_quadVertices[0],
               GL_STATIC_DRAW);

  glGenBuffers(1, &g_ibID);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ibID);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               36 * sizeof(char),
               (void*)&g_Indices[0],
               GL_STATIC_DRAW);

  //
  // Create a texture to test out our fragment shader...
  //
  glGenTextures(1, &g_textureID);
  glBindTexture(GL_TEXTURE_2D, g_textureID);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 128, 128, 0, GL_RGB, GL_UNSIGNED_BYTE,
               g_TextureData);

  //
  // Locate some parameters by name so we can set them later...
  //
  g_textureLoc = glGetUniformLocation(g_programObj, "arrowTexture");
  g_positionLoc = glGetAttribLocation(g_programObj, "a_position");
  g_texCoordLoc = glGetAttribLocation(g_programObj, "a_texCoord");
  g_colorLoc = glGetAttribLocation(g_programObj, "a_color");
  g_MVPLoc = glGetUniformLocation(g_programObj, "a_MVP");
  printf("Program initialized.\n");
}

void BuildQuad(Vertex* verts, int axis[3], float depth, float color[3]) {
  static float X[4] = { -1.0f, 1.0f, 1.0f, -1.0f };
  static float Y[4] = { -1.0f, -1.0f, 1.0f, 1.0f };

  for (int i = 0; i < 4; i++) {
    verts[i].tu = (1.0 - X[i]) / 2.0f;
    verts[i].tv = (Y[i] + 1.0f) / -2.0f * depth;
    verts[i].loc[axis[0]] = X[i] * depth;
    verts[i].loc[axis[1]] = Y[i] * depth;
    verts[i].loc[axis[2]] = depth;
    for (int j = 0; j < 3; j++)
      verts[i].color[j] = color[j] * (Y[i] + 1.0f) / 2.0f;
  }
}

Vertex* BuildCube() {
  Vertex* verts = new Vertex[24];
  for (int i = 0; i < 3; i++) {
    int Faxis[3];
    int Baxis[3];
    float Fcolor[3];
    float Bcolor[3];
    for (int j = 0; j < 3; j++) {
      Faxis[j] = (j + i) % 3;
      Baxis[j] = (j + i) % 3;
    }
    memset(Fcolor, 0, sizeof(float) * 3);
    memset(Bcolor, 0, sizeof(float) * 3);
    Fcolor[i] = 0.5f;
    Bcolor[i] = 1.0f;
    BuildQuad(&verts[0 + i * 4], Faxis, 1.0f, Fcolor);
    BuildQuad(&verts[12 + i * 4], Baxis, -1.0f, Bcolor);
  }

  for (int i = 0; i < 6; i++) {
    g_Indices[i * 6 + 0] = 2 + i * 4;
    g_Indices[i * 6 + 1] = 1 + i * 4;
    g_Indices[i * 6 + 2] = 0 + i * 4;
    g_Indices[i * 6 + 3] = 3 + i * 4;
    g_Indices[i * 6 + 4] = 2 + i * 4;
    g_Indices[i * 6 + 5] = 0 + i * 4;
  }
  return verts;
}

static float clamp(float val, float min, float max) {
  if (val < min)
    return min;
  if (val > max)
    return max;
  return val;
}

void ProcessEvent(PPAPIEvent* event) {
  if (event->event_type == PP_INPUTEVENT_TYPE_MOUSEMOVE) {
    PPAPIMouseEvent* mouse = (PPAPIMouseEvent*)event;
    g_ySpin = clamp((float) mouse->delta.x / 2, -4.0, 4.0);
    g_xSpin = clamp((float) mouse->delta.y / 2, -4.0, 4.0);
  }
  if (event->event_type == PP_INPUTEVENT_TYPE_KEYUP) {
    PPAPIKeyEvent* key = (PPAPIKeyEvent*)event;
    if (key->key_code == 13) {
      PPAPIInstance3D::GetInstance3D()->ToggleFullscreen();
    }
  }
}

void PPAPIRender(PP_Resource ctx, uint32_t width, uint32_t height) {
  if (!g_Ready) {
    if (g_Loaded) {
      InitProgram();
      g_Ready = true;
    } else {
      return;
    }
  }

  PPAPIEvent* event;
  while (PPAPIEvent* event = PPAPI_AcquireEvent()) {
    ProcessEvent(event);
    PPAPI_ReleaseEvent(event);
  }

  static float xRot = 0.0;
  static float yRot = 0.0;

  xRot -= g_xSpin;
  yRot -= g_ySpin;

  if (xRot >= 360.0f)
    xRot = 0.0;
  if (xRot <= -360.0f)
    xRot = 0.0;

  if (yRot >= 360.0f)
    yRot = 0.0;
  if (yRot <= -360.0f)
    yRot = 0.0;

  glClearColor(0.5, 0.5, 0.5, 1);
  glClearDepthf(1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);

  //set what program to use
  glUseProgram(g_programObj);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, g_textureID);
  glUniform1i(g_textureLoc, 0);

  //create our perspective matrix
  float mpv[16];
  float trs[16];
  float rot[16];

  identity_matrix(mpv);
  glhPerspectivef2(&mpv[0], 45.0f, (float)(width) / (float) height, 1, 10);

  translate_matrix(0, 0, -4.0, trs);
  rotate_matrix(xRot, yRot, 0.0f, rot);
  multiply_matrix(trs, rot, trs);
  multiply_matrix(mpv, trs, mpv);
  glUniformMatrix4fv(g_MVPLoc, 1, GL_FALSE, (GLfloat*)mpv);

  //define the attributes of the vertex
  glBindBuffer(GL_ARRAY_BUFFER, g_vboID);
  glVertexAttribPointer(g_positionLoc,
                        3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(Vertex),
                        (void*)offsetof(Vertex, loc));
  glEnableVertexAttribArray(g_positionLoc);
  glVertexAttribPointer(g_texCoordLoc,
                        2,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(Vertex),
                        (void*)offsetof(Vertex, tu));
  glEnableVertexAttribArray(g_texCoordLoc);
  glVertexAttribPointer(g_colorLoc,
                        3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(Vertex),
                        (void*)offsetof(Vertex, color));
  glEnableVertexAttribArray(g_colorLoc);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ibID);
  glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_BYTE, 0);
}

const char* LoadData(const char* url) {
  char* buf;
  struct stat stat_buf;

  int fp = open(url, O_RDONLY);
  fstat(fp, &stat_buf);

  int len = static_cast<int>(stat_buf.st_size);
  buf = new char[len + 1];
  int read_size = read(fp, buf, len);
  buf[len] = 0;
  return buf;
}

PPAPI_MAIN_USE(PPAPI_CreateInstance3D, PPAPI_MAIN_DEFAULT_ARGS)
int ppapi_main(int argc, const char* argv[]) {
  printf("Started main.\n");

  // Mount URL loads to /http
  mount("", "/http", "httpfs", 0, "");

  g_TextureData = LoadData("/http/hello.raw");
  g_VShaderData = LoadData("/http/vertex_shader_es2.vert");
  g_FShaderData = LoadData("/http/fragment_shader_es2.frag");
  g_quadVertices = BuildCube();

  fprintf(stderr, "Loaded\n");
  g_Loaded = true;
  return 0;
}
