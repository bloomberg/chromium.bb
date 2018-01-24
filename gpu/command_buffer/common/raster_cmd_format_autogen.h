// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_raster_cmd_buffer.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_AUTOGEN_H_

#define GL_SCANOUT_CHROMIUM 0x6000

struct ActiveTexture {
  typedef ActiveTexture ValueType;
  static const CommandId kCmdId = kActiveTexture;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _texture) {
    SetHeader();
    texture = _texture;
  }

  void* Set(void* cmd, GLenum _texture) {
    static_cast<ValueType*>(cmd)->Init(_texture);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture;
};

static_assert(sizeof(ActiveTexture) == 8, "size of ActiveTexture should be 8");
static_assert(offsetof(ActiveTexture, header) == 0,
              "offset of ActiveTexture header should be 0");
static_assert(offsetof(ActiveTexture, texture) == 4,
              "offset of ActiveTexture texture should be 4");

struct BindTexture {
  typedef BindTexture ValueType;
  static const CommandId kCmdId = kBindTexture;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLuint _texture) {
    SetHeader();
    target = _target;
    texture = _texture;
  }

  void* Set(void* cmd, GLenum _target, GLuint _texture) {
    static_cast<ValueType*>(cmd)->Init(_target, _texture);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t texture;
};

static_assert(sizeof(BindTexture) == 12, "size of BindTexture should be 12");
static_assert(offsetof(BindTexture, header) == 0,
              "offset of BindTexture header should be 0");
static_assert(offsetof(BindTexture, target) == 4,
              "offset of BindTexture target should be 4");
static_assert(offsetof(BindTexture, texture) == 8,
              "offset of BindTexture texture should be 8");

struct CompressedTexImage2DBucket {
  typedef CompressedTexImage2DBucket ValueType;
  static const CommandId kCmdId = kCompressedTexImage2DBucket;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLuint _bucket_id) {
    SetHeader();
    target = _target;
    level = _level;
    internalformat = _internalformat;
    width = _width;
    height = _height;
    bucket_id = _bucket_id;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLuint _bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_target, _level, _internalformat, _width,
                                       _height, _bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  uint32_t internalformat;
  int32_t width;
  int32_t height;
  uint32_t bucket_id;
  static const int32_t border = 0;
};

static_assert(sizeof(CompressedTexImage2DBucket) == 28,
              "size of CompressedTexImage2DBucket should be 28");
static_assert(offsetof(CompressedTexImage2DBucket, header) == 0,
              "offset of CompressedTexImage2DBucket header should be 0");
static_assert(offsetof(CompressedTexImage2DBucket, target) == 4,
              "offset of CompressedTexImage2DBucket target should be 4");
static_assert(offsetof(CompressedTexImage2DBucket, level) == 8,
              "offset of CompressedTexImage2DBucket level should be 8");
static_assert(
    offsetof(CompressedTexImage2DBucket, internalformat) == 12,
    "offset of CompressedTexImage2DBucket internalformat should be 12");
static_assert(offsetof(CompressedTexImage2DBucket, width) == 16,
              "offset of CompressedTexImage2DBucket width should be 16");
static_assert(offsetof(CompressedTexImage2DBucket, height) == 20,
              "offset of CompressedTexImage2DBucket height should be 20");
static_assert(offsetof(CompressedTexImage2DBucket, bucket_id) == 24,
              "offset of CompressedTexImage2DBucket bucket_id should be 24");

struct CompressedTexImage2D {
  typedef CompressedTexImage2D ValueType;
  static const CommandId kCmdId = kCompressedTexImage2D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLsizei _imageSize,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset) {
    SetHeader();
    target = _target;
    level = _level;
    internalformat = _internalformat;
    width = _width;
    height = _height;
    imageSize = _imageSize;
    data_shm_id = _data_shm_id;
    data_shm_offset = _data_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLenum _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLsizei _imageSize,
            uint32_t _data_shm_id,
            uint32_t _data_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _level, _internalformat, _width,
                                       _height, _imageSize, _data_shm_id,
                                       _data_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  uint32_t internalformat;
  int32_t width;
  int32_t height;
  int32_t imageSize;
  uint32_t data_shm_id;
  uint32_t data_shm_offset;
  static const int32_t border = 0;
};

static_assert(sizeof(CompressedTexImage2D) == 36,
              "size of CompressedTexImage2D should be 36");
static_assert(offsetof(CompressedTexImage2D, header) == 0,
              "offset of CompressedTexImage2D header should be 0");
static_assert(offsetof(CompressedTexImage2D, target) == 4,
              "offset of CompressedTexImage2D target should be 4");
static_assert(offsetof(CompressedTexImage2D, level) == 8,
              "offset of CompressedTexImage2D level should be 8");
static_assert(offsetof(CompressedTexImage2D, internalformat) == 12,
              "offset of CompressedTexImage2D internalformat should be 12");
static_assert(offsetof(CompressedTexImage2D, width) == 16,
              "offset of CompressedTexImage2D width should be 16");
static_assert(offsetof(CompressedTexImage2D, height) == 20,
              "offset of CompressedTexImage2D height should be 20");
static_assert(offsetof(CompressedTexImage2D, imageSize) == 24,
              "offset of CompressedTexImage2D imageSize should be 24");
static_assert(offsetof(CompressedTexImage2D, data_shm_id) == 28,
              "offset of CompressedTexImage2D data_shm_id should be 28");
static_assert(offsetof(CompressedTexImage2D, data_shm_offset) == 32,
              "offset of CompressedTexImage2D data_shm_offset should be 32");

struct DeleteTexturesImmediate {
  typedef DeleteTexturesImmediate ValueType;
  static const CommandId kCmdId = kDeleteTexturesImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, const GLuint* _textures) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _textures, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, const GLuint* _textures) {
    static_cast<ValueType*>(cmd)->Init(_n, _textures);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(DeleteTexturesImmediate) == 8,
              "size of DeleteTexturesImmediate should be 8");
static_assert(offsetof(DeleteTexturesImmediate, header) == 0,
              "offset of DeleteTexturesImmediate header should be 0");
static_assert(offsetof(DeleteTexturesImmediate, n) == 4,
              "offset of DeleteTexturesImmediate n should be 4");

struct Finish {
  typedef Finish ValueType;
  static const CommandId kCmdId = kFinish;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(Finish) == 4, "size of Finish should be 4");
static_assert(offsetof(Finish, header) == 0,
              "offset of Finish header should be 0");

struct Flush {
  typedef Flush ValueType;
  static const CommandId kCmdId = kFlush;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(Flush) == 4, "size of Flush should be 4");
static_assert(offsetof(Flush, header) == 0,
              "offset of Flush header should be 0");

struct GenTexturesImmediate {
  typedef GenTexturesImmediate ValueType;
  static const CommandId kCmdId = kGenTexturesImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, GLuint* _textures) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _textures, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, GLuint* _textures) {
    static_cast<ValueType*>(cmd)->Init(_n, _textures);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(GenTexturesImmediate) == 8,
              "size of GenTexturesImmediate should be 8");
static_assert(offsetof(GenTexturesImmediate, header) == 0,
              "offset of GenTexturesImmediate header should be 0");
static_assert(offsetof(GenTexturesImmediate, n) == 4,
              "offset of GenTexturesImmediate n should be 4");

struct GetError {
  typedef GetError ValueType;
  static const CommandId kCmdId = kGetError;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef GLenum Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(uint32_t _result_shm_id, uint32_t _result_shm_offset) {
    SetHeader();
    result_shm_id = _result_shm_id;
    result_shm_offset = _result_shm_offset;
  }

  void* Set(void* cmd, uint32_t _result_shm_id, uint32_t _result_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_result_shm_id, _result_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t result_shm_id;
  uint32_t result_shm_offset;
};

static_assert(sizeof(GetError) == 12, "size of GetError should be 12");
static_assert(offsetof(GetError, header) == 0,
              "offset of GetError header should be 0");
static_assert(offsetof(GetError, result_shm_id) == 4,
              "offset of GetError result_shm_id should be 4");
static_assert(offsetof(GetError, result_shm_offset) == 8,
              "offset of GetError result_shm_offset should be 8");

struct GetIntegerv {
  typedef GetIntegerv ValueType;
  static const CommandId kCmdId = kGetIntegerv;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  typedef SizedResult<GLint> Result;

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    SetHeader();
    pname = _pname;
    params_shm_id = _params_shm_id;
    params_shm_offset = _params_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _pname,
            uint32_t _params_shm_id,
            uint32_t _params_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_pname, _params_shm_id,
                                       _params_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t pname;
  uint32_t params_shm_id;
  uint32_t params_shm_offset;
};

static_assert(sizeof(GetIntegerv) == 16, "size of GetIntegerv should be 16");
static_assert(offsetof(GetIntegerv, header) == 0,
              "offset of GetIntegerv header should be 0");
static_assert(offsetof(GetIntegerv, pname) == 4,
              "offset of GetIntegerv pname should be 4");
static_assert(offsetof(GetIntegerv, params_shm_id) == 8,
              "offset of GetIntegerv params_shm_id should be 8");
static_assert(offsetof(GetIntegerv, params_shm_offset) == 12,
              "offset of GetIntegerv params_shm_offset should be 12");

struct PixelStorei {
  typedef PixelStorei ValueType;
  static const CommandId kCmdId = kPixelStorei;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _pname, GLint _param) {
    SetHeader();
    pname = _pname;
    param = _param;
  }

  void* Set(void* cmd, GLenum _pname, GLint _param) {
    static_cast<ValueType*>(cmd)->Init(_pname, _param);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t pname;
  int32_t param;
};

static_assert(sizeof(PixelStorei) == 12, "size of PixelStorei should be 12");
static_assert(offsetof(PixelStorei, header) == 0,
              "offset of PixelStorei header should be 0");
static_assert(offsetof(PixelStorei, pname) == 4,
              "offset of PixelStorei pname should be 4");
static_assert(offsetof(PixelStorei, param) == 8,
              "offset of PixelStorei param should be 8");

struct TexImage2D {
  typedef TexImage2D ValueType;
  static const CommandId kCmdId = kTexImage2D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLint _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLenum _format,
            GLenum _type,
            uint32_t _pixels_shm_id,
            uint32_t _pixels_shm_offset) {
    SetHeader();
    target = _target;
    level = _level;
    internalformat = _internalformat;
    width = _width;
    height = _height;
    format = _format;
    type = _type;
    pixels_shm_id = _pixels_shm_id;
    pixels_shm_offset = _pixels_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLint _internalformat,
            GLsizei _width,
            GLsizei _height,
            GLenum _format,
            GLenum _type,
            uint32_t _pixels_shm_id,
            uint32_t _pixels_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _level, _internalformat, _width,
                                       _height, _format, _type, _pixels_shm_id,
                                       _pixels_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  int32_t internalformat;
  int32_t width;
  int32_t height;
  uint32_t format;
  uint32_t type;
  uint32_t pixels_shm_id;
  uint32_t pixels_shm_offset;
  static const int32_t border = 0;
};

static_assert(sizeof(TexImage2D) == 40, "size of TexImage2D should be 40");
static_assert(offsetof(TexImage2D, header) == 0,
              "offset of TexImage2D header should be 0");
static_assert(offsetof(TexImage2D, target) == 4,
              "offset of TexImage2D target should be 4");
static_assert(offsetof(TexImage2D, level) == 8,
              "offset of TexImage2D level should be 8");
static_assert(offsetof(TexImage2D, internalformat) == 12,
              "offset of TexImage2D internalformat should be 12");
static_assert(offsetof(TexImage2D, width) == 16,
              "offset of TexImage2D width should be 16");
static_assert(offsetof(TexImage2D, height) == 20,
              "offset of TexImage2D height should be 20");
static_assert(offsetof(TexImage2D, format) == 24,
              "offset of TexImage2D format should be 24");
static_assert(offsetof(TexImage2D, type) == 28,
              "offset of TexImage2D type should be 28");
static_assert(offsetof(TexImage2D, pixels_shm_id) == 32,
              "offset of TexImage2D pixels_shm_id should be 32");
static_assert(offsetof(TexImage2D, pixels_shm_offset) == 36,
              "offset of TexImage2D pixels_shm_offset should be 36");

struct TexParameteri {
  typedef TexParameteri ValueType;
  static const CommandId kCmdId = kTexParameteri;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLenum _pname, GLint _param) {
    SetHeader();
    target = _target;
    pname = _pname;
    param = _param;
  }

  void* Set(void* cmd, GLenum _target, GLenum _pname, GLint _param) {
    static_cast<ValueType*>(cmd)->Init(_target, _pname, _param);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t pname;
  int32_t param;
};

static_assert(sizeof(TexParameteri) == 16,
              "size of TexParameteri should be 16");
static_assert(offsetof(TexParameteri, header) == 0,
              "offset of TexParameteri header should be 0");
static_assert(offsetof(TexParameteri, target) == 4,
              "offset of TexParameteri target should be 4");
static_assert(offsetof(TexParameteri, pname) == 8,
              "offset of TexParameteri pname should be 8");
static_assert(offsetof(TexParameteri, param) == 12,
              "offset of TexParameteri param should be 12");

struct TexSubImage2D {
  typedef TexSubImage2D ValueType;
  static const CommandId kCmdId = kTexSubImage2D;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLsizei _width,
            GLsizei _height,
            GLenum _format,
            GLenum _type,
            uint32_t _pixels_shm_id,
            uint32_t _pixels_shm_offset,
            GLboolean _internal) {
    SetHeader();
    target = _target;
    level = _level;
    xoffset = _xoffset;
    yoffset = _yoffset;
    width = _width;
    height = _height;
    format = _format;
    type = _type;
    pixels_shm_id = _pixels_shm_id;
    pixels_shm_offset = _pixels_shm_offset;
    internal = _internal;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLint _level,
            GLint _xoffset,
            GLint _yoffset,
            GLsizei _width,
            GLsizei _height,
            GLenum _format,
            GLenum _type,
            uint32_t _pixels_shm_id,
            uint32_t _pixels_shm_offset,
            GLboolean _internal) {
    static_cast<ValueType*>(cmd)->Init(
        _target, _level, _xoffset, _yoffset, _width, _height, _format, _type,
        _pixels_shm_id, _pixels_shm_offset, _internal);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t level;
  int32_t xoffset;
  int32_t yoffset;
  int32_t width;
  int32_t height;
  uint32_t format;
  uint32_t type;
  uint32_t pixels_shm_id;
  uint32_t pixels_shm_offset;
  uint32_t internal;
};

static_assert(sizeof(TexSubImage2D) == 48,
              "size of TexSubImage2D should be 48");
static_assert(offsetof(TexSubImage2D, header) == 0,
              "offset of TexSubImage2D header should be 0");
static_assert(offsetof(TexSubImage2D, target) == 4,
              "offset of TexSubImage2D target should be 4");
static_assert(offsetof(TexSubImage2D, level) == 8,
              "offset of TexSubImage2D level should be 8");
static_assert(offsetof(TexSubImage2D, xoffset) == 12,
              "offset of TexSubImage2D xoffset should be 12");
static_assert(offsetof(TexSubImage2D, yoffset) == 16,
              "offset of TexSubImage2D yoffset should be 16");
static_assert(offsetof(TexSubImage2D, width) == 20,
              "offset of TexSubImage2D width should be 20");
static_assert(offsetof(TexSubImage2D, height) == 24,
              "offset of TexSubImage2D height should be 24");
static_assert(offsetof(TexSubImage2D, format) == 28,
              "offset of TexSubImage2D format should be 28");
static_assert(offsetof(TexSubImage2D, type) == 32,
              "offset of TexSubImage2D type should be 32");
static_assert(offsetof(TexSubImage2D, pixels_shm_id) == 36,
              "offset of TexSubImage2D pixels_shm_id should be 36");
static_assert(offsetof(TexSubImage2D, pixels_shm_offset) == 40,
              "offset of TexSubImage2D pixels_shm_offset should be 40");
static_assert(offsetof(TexSubImage2D, internal) == 44,
              "offset of TexSubImage2D internal should be 44");

struct TexStorage2DEXT {
  typedef TexStorage2DEXT ValueType;
  static const CommandId kCmdId = kTexStorage2DEXT;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLsizei _levels,
            GLenum _internalFormat,
            GLsizei _width,
            GLsizei _height) {
    SetHeader();
    target = _target;
    levels = _levels;
    internalFormat = _internalFormat;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLsizei _levels,
            GLenum _internalFormat,
            GLsizei _width,
            GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_target, _levels, _internalFormat,
                                       _width, _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t levels;
  uint32_t internalFormat;
  int32_t width;
  int32_t height;
};

static_assert(sizeof(TexStorage2DEXT) == 24,
              "size of TexStorage2DEXT should be 24");
static_assert(offsetof(TexStorage2DEXT, header) == 0,
              "offset of TexStorage2DEXT header should be 0");
static_assert(offsetof(TexStorage2DEXT, target) == 4,
              "offset of TexStorage2DEXT target should be 4");
static_assert(offsetof(TexStorage2DEXT, levels) == 8,
              "offset of TexStorage2DEXT levels should be 8");
static_assert(offsetof(TexStorage2DEXT, internalFormat) == 12,
              "offset of TexStorage2DEXT internalFormat should be 12");
static_assert(offsetof(TexStorage2DEXT, width) == 16,
              "offset of TexStorage2DEXT width should be 16");
static_assert(offsetof(TexStorage2DEXT, height) == 20,
              "offset of TexStorage2DEXT height should be 20");

struct GenQueriesEXTImmediate {
  typedef GenQueriesEXTImmediate ValueType;
  static const CommandId kCmdId = kGenQueriesEXTImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, GLuint* _queries) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _queries, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, GLuint* _queries) {
    static_cast<ValueType*>(cmd)->Init(_n, _queries);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(GenQueriesEXTImmediate) == 8,
              "size of GenQueriesEXTImmediate should be 8");
static_assert(offsetof(GenQueriesEXTImmediate, header) == 0,
              "offset of GenQueriesEXTImmediate header should be 0");
static_assert(offsetof(GenQueriesEXTImmediate, n) == 4,
              "offset of GenQueriesEXTImmediate n should be 4");

struct DeleteQueriesEXTImmediate {
  typedef DeleteQueriesEXTImmediate ValueType;
  static const CommandId kCmdId = kDeleteQueriesEXTImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeDataSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(GLuint) * _n);  // NOLINT
  }

  static uint32_t ComputeSize(GLsizei _n) {
    return static_cast<uint32_t>(sizeof(ValueType) +
                                 ComputeDataSize(_n));  // NOLINT
  }

  void SetHeader(GLsizei _n) {
    header.SetCmdByTotalSize<ValueType>(ComputeSize(_n));
  }

  void Init(GLsizei _n, const GLuint* _queries) {
    SetHeader(_n);
    n = _n;
    memcpy(ImmediateDataAddress(this), _queries, ComputeDataSize(_n));
  }

  void* Set(void* cmd, GLsizei _n, const GLuint* _queries) {
    static_cast<ValueType*>(cmd)->Init(_n, _queries);
    const uint32_t size = ComputeSize(_n);
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  int32_t n;
};

static_assert(sizeof(DeleteQueriesEXTImmediate) == 8,
              "size of DeleteQueriesEXTImmediate should be 8");
static_assert(offsetof(DeleteQueriesEXTImmediate, header) == 0,
              "offset of DeleteQueriesEXTImmediate header should be 0");
static_assert(offsetof(DeleteQueriesEXTImmediate, n) == 4,
              "offset of DeleteQueriesEXTImmediate n should be 4");

struct BeginQueryEXT {
  typedef BeginQueryEXT ValueType;
  static const CommandId kCmdId = kBeginQueryEXT;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLuint _id,
            uint32_t _sync_data_shm_id,
            uint32_t _sync_data_shm_offset) {
    SetHeader();
    target = _target;
    id = _id;
    sync_data_shm_id = _sync_data_shm_id;
    sync_data_shm_offset = _sync_data_shm_offset;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLuint _id,
            uint32_t _sync_data_shm_id,
            uint32_t _sync_data_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_target, _id, _sync_data_shm_id,
                                       _sync_data_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t id;
  uint32_t sync_data_shm_id;
  uint32_t sync_data_shm_offset;
};

static_assert(sizeof(BeginQueryEXT) == 20,
              "size of BeginQueryEXT should be 20");
static_assert(offsetof(BeginQueryEXT, header) == 0,
              "offset of BeginQueryEXT header should be 0");
static_assert(offsetof(BeginQueryEXT, target) == 4,
              "offset of BeginQueryEXT target should be 4");
static_assert(offsetof(BeginQueryEXT, id) == 8,
              "offset of BeginQueryEXT id should be 8");
static_assert(offsetof(BeginQueryEXT, sync_data_shm_id) == 12,
              "offset of BeginQueryEXT sync_data_shm_id should be 12");
static_assert(offsetof(BeginQueryEXT, sync_data_shm_offset) == 16,
              "offset of BeginQueryEXT sync_data_shm_offset should be 16");

struct EndQueryEXT {
  typedef EndQueryEXT ValueType;
  static const CommandId kCmdId = kEndQueryEXT;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLuint _submit_count) {
    SetHeader();
    target = _target;
    submit_count = _submit_count;
  }

  void* Set(void* cmd, GLenum _target, GLuint _submit_count) {
    static_cast<ValueType*>(cmd)->Init(_target, _submit_count);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t submit_count;
};

static_assert(sizeof(EndQueryEXT) == 12, "size of EndQueryEXT should be 12");
static_assert(offsetof(EndQueryEXT, header) == 0,
              "offset of EndQueryEXT header should be 0");
static_assert(offsetof(EndQueryEXT, target) == 4,
              "offset of EndQueryEXT target should be 4");
static_assert(offsetof(EndQueryEXT, submit_count) == 8,
              "offset of EndQueryEXT submit_count should be 8");

struct CopySubTextureCHROMIUM {
  typedef CopySubTextureCHROMIUM ValueType;
  static const CommandId kCmdId = kCopySubTextureCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(2);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _source_id,
            GLint _source_level,
            GLenum _dest_target,
            GLuint _dest_id,
            GLint _dest_level,
            GLint _xoffset,
            GLint _yoffset,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height,
            GLboolean _unpack_flip_y,
            GLboolean _unpack_premultiply_alpha,
            GLboolean _unpack_unmultiply_alpha) {
    SetHeader();
    source_id = _source_id;
    source_level = _source_level;
    dest_target = _dest_target;
    dest_id = _dest_id;
    dest_level = _dest_level;
    xoffset = _xoffset;
    yoffset = _yoffset;
    x = _x;
    y = _y;
    width = _width;
    height = _height;
    unpack_flip_y = _unpack_flip_y;
    unpack_premultiply_alpha = _unpack_premultiply_alpha;
    unpack_unmultiply_alpha = _unpack_unmultiply_alpha;
  }

  void* Set(void* cmd,
            GLuint _source_id,
            GLint _source_level,
            GLenum _dest_target,
            GLuint _dest_id,
            GLint _dest_level,
            GLint _xoffset,
            GLint _yoffset,
            GLint _x,
            GLint _y,
            GLsizei _width,
            GLsizei _height,
            GLboolean _unpack_flip_y,
            GLboolean _unpack_premultiply_alpha,
            GLboolean _unpack_unmultiply_alpha) {
    static_cast<ValueType*>(cmd)->Init(
        _source_id, _source_level, _dest_target, _dest_id, _dest_level,
        _xoffset, _yoffset, _x, _y, _width, _height, _unpack_flip_y,
        _unpack_premultiply_alpha, _unpack_unmultiply_alpha);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t source_id;
  int32_t source_level;
  uint32_t dest_target;
  uint32_t dest_id;
  int32_t dest_level;
  int32_t xoffset;
  int32_t yoffset;
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
  uint32_t unpack_flip_y;
  uint32_t unpack_premultiply_alpha;
  uint32_t unpack_unmultiply_alpha;
};

static_assert(sizeof(CopySubTextureCHROMIUM) == 60,
              "size of CopySubTextureCHROMIUM should be 60");
static_assert(offsetof(CopySubTextureCHROMIUM, header) == 0,
              "offset of CopySubTextureCHROMIUM header should be 0");
static_assert(offsetof(CopySubTextureCHROMIUM, source_id) == 4,
              "offset of CopySubTextureCHROMIUM source_id should be 4");
static_assert(offsetof(CopySubTextureCHROMIUM, source_level) == 8,
              "offset of CopySubTextureCHROMIUM source_level should be 8");
static_assert(offsetof(CopySubTextureCHROMIUM, dest_target) == 12,
              "offset of CopySubTextureCHROMIUM dest_target should be 12");
static_assert(offsetof(CopySubTextureCHROMIUM, dest_id) == 16,
              "offset of CopySubTextureCHROMIUM dest_id should be 16");
static_assert(offsetof(CopySubTextureCHROMIUM, dest_level) == 20,
              "offset of CopySubTextureCHROMIUM dest_level should be 20");
static_assert(offsetof(CopySubTextureCHROMIUM, xoffset) == 24,
              "offset of CopySubTextureCHROMIUM xoffset should be 24");
static_assert(offsetof(CopySubTextureCHROMIUM, yoffset) == 28,
              "offset of CopySubTextureCHROMIUM yoffset should be 28");
static_assert(offsetof(CopySubTextureCHROMIUM, x) == 32,
              "offset of CopySubTextureCHROMIUM x should be 32");
static_assert(offsetof(CopySubTextureCHROMIUM, y) == 36,
              "offset of CopySubTextureCHROMIUM y should be 36");
static_assert(offsetof(CopySubTextureCHROMIUM, width) == 40,
              "offset of CopySubTextureCHROMIUM width should be 40");
static_assert(offsetof(CopySubTextureCHROMIUM, height) == 44,
              "offset of CopySubTextureCHROMIUM height should be 44");
static_assert(offsetof(CopySubTextureCHROMIUM, unpack_flip_y) == 48,
              "offset of CopySubTextureCHROMIUM unpack_flip_y should be 48");
static_assert(
    offsetof(CopySubTextureCHROMIUM, unpack_premultiply_alpha) == 52,
    "offset of CopySubTextureCHROMIUM unpack_premultiply_alpha should be 52");
static_assert(
    offsetof(CopySubTextureCHROMIUM, unpack_unmultiply_alpha) == 56,
    "offset of CopySubTextureCHROMIUM unpack_unmultiply_alpha should be 56");

struct CompressedCopyTextureCHROMIUM {
  typedef CompressedCopyTextureCHROMIUM ValueType;
  static const CommandId kCmdId = kCompressedCopyTextureCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _source_id, GLuint _dest_id) {
    SetHeader();
    source_id = _source_id;
    dest_id = _dest_id;
  }

  void* Set(void* cmd, GLuint _source_id, GLuint _dest_id) {
    static_cast<ValueType*>(cmd)->Init(_source_id, _dest_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t source_id;
  uint32_t dest_id;
};

static_assert(sizeof(CompressedCopyTextureCHROMIUM) == 12,
              "size of CompressedCopyTextureCHROMIUM should be 12");
static_assert(offsetof(CompressedCopyTextureCHROMIUM, header) == 0,
              "offset of CompressedCopyTextureCHROMIUM header should be 0");
static_assert(offsetof(CompressedCopyTextureCHROMIUM, source_id) == 4,
              "offset of CompressedCopyTextureCHROMIUM source_id should be 4");
static_assert(offsetof(CompressedCopyTextureCHROMIUM, dest_id) == 8,
              "offset of CompressedCopyTextureCHROMIUM dest_id should be 8");

struct ProduceTextureDirectCHROMIUMImmediate {
  typedef ProduceTextureDirectCHROMIUMImmediate ValueType;
  static const CommandId kCmdId = kProduceTextureDirectCHROMIUMImmediate;
  static const cmd::ArgFlags kArgFlags = cmd::kAtLeastN;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeDataSize() {
    return static_cast<uint32_t>(sizeof(GLbyte) * 16);
  }

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType) + ComputeDataSize());
  }

  void SetHeader() { header.SetCmdByTotalSize<ValueType>(ComputeSize()); }

  void Init(GLuint _texture, const GLbyte* _mailbox) {
    SetHeader();
    texture = _texture;
    memcpy(ImmediateDataAddress(this), _mailbox, ComputeDataSize());
  }

  void* Set(void* cmd, GLuint _texture, const GLbyte* _mailbox) {
    static_cast<ValueType*>(cmd)->Init(_texture, _mailbox);
    const uint32_t size = ComputeSize();
    return NextImmediateCmdAddressTotalSize<ValueType>(cmd, size);
  }

  gpu::CommandHeader header;
  uint32_t texture;
};

static_assert(sizeof(ProduceTextureDirectCHROMIUMImmediate) == 8,
              "size of ProduceTextureDirectCHROMIUMImmediate should be 8");
static_assert(
    offsetof(ProduceTextureDirectCHROMIUMImmediate, header) == 0,
    "offset of ProduceTextureDirectCHROMIUMImmediate header should be 0");
static_assert(
    offsetof(ProduceTextureDirectCHROMIUMImmediate, texture) == 4,
    "offset of ProduceTextureDirectCHROMIUMImmediate texture should be 4");

struct BindTexImage2DCHROMIUM {
  typedef BindTexImage2DCHROMIUM ValueType;
  static const CommandId kCmdId = kBindTexImage2DCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLint _imageId) {
    SetHeader();
    target = _target;
    imageId = _imageId;
  }

  void* Set(void* cmd, GLenum _target, GLint _imageId) {
    static_cast<ValueType*>(cmd)->Init(_target, _imageId);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t imageId;
};

static_assert(sizeof(BindTexImage2DCHROMIUM) == 12,
              "size of BindTexImage2DCHROMIUM should be 12");
static_assert(offsetof(BindTexImage2DCHROMIUM, header) == 0,
              "offset of BindTexImage2DCHROMIUM header should be 0");
static_assert(offsetof(BindTexImage2DCHROMIUM, target) == 4,
              "offset of BindTexImage2DCHROMIUM target should be 4");
static_assert(offsetof(BindTexImage2DCHROMIUM, imageId) == 8,
              "offset of BindTexImage2DCHROMIUM imageId should be 8");

struct ReleaseTexImage2DCHROMIUM {
  typedef ReleaseTexImage2DCHROMIUM ValueType;
  static const CommandId kCmdId = kReleaseTexImage2DCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target, GLint _imageId) {
    SetHeader();
    target = _target;
    imageId = _imageId;
  }

  void* Set(void* cmd, GLenum _target, GLint _imageId) {
    static_cast<ValueType*>(cmd)->Init(_target, _imageId);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  int32_t imageId;
};

static_assert(sizeof(ReleaseTexImage2DCHROMIUM) == 12,
              "size of ReleaseTexImage2DCHROMIUM should be 12");
static_assert(offsetof(ReleaseTexImage2DCHROMIUM, header) == 0,
              "offset of ReleaseTexImage2DCHROMIUM header should be 0");
static_assert(offsetof(ReleaseTexImage2DCHROMIUM, target) == 4,
              "offset of ReleaseTexImage2DCHROMIUM target should be 4");
static_assert(offsetof(ReleaseTexImage2DCHROMIUM, imageId) == 8,
              "offset of ReleaseTexImage2DCHROMIUM imageId should be 8");

struct TraceBeginCHROMIUM {
  typedef TraceBeginCHROMIUM ValueType;
  static const CommandId kCmdId = kTraceBeginCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _category_bucket_id, GLuint _name_bucket_id) {
    SetHeader();
    category_bucket_id = _category_bucket_id;
    name_bucket_id = _name_bucket_id;
  }

  void* Set(void* cmd, GLuint _category_bucket_id, GLuint _name_bucket_id) {
    static_cast<ValueType*>(cmd)->Init(_category_bucket_id, _name_bucket_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t category_bucket_id;
  uint32_t name_bucket_id;
};

static_assert(sizeof(TraceBeginCHROMIUM) == 12,
              "size of TraceBeginCHROMIUM should be 12");
static_assert(offsetof(TraceBeginCHROMIUM, header) == 0,
              "offset of TraceBeginCHROMIUM header should be 0");
static_assert(offsetof(TraceBeginCHROMIUM, category_bucket_id) == 4,
              "offset of TraceBeginCHROMIUM category_bucket_id should be 4");
static_assert(offsetof(TraceBeginCHROMIUM, name_bucket_id) == 8,
              "offset of TraceBeginCHROMIUM name_bucket_id should be 8");

struct TraceEndCHROMIUM {
  typedef TraceEndCHROMIUM ValueType;
  static const CommandId kCmdId = kTraceEndCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(TraceEndCHROMIUM) == 4,
              "size of TraceEndCHROMIUM should be 4");
static_assert(offsetof(TraceEndCHROMIUM, header) == 0,
              "offset of TraceEndCHROMIUM header should be 0");

struct LoseContextCHROMIUM {
  typedef LoseContextCHROMIUM ValueType;
  static const CommandId kCmdId = kLoseContextCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(1);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _current, GLenum _other) {
    SetHeader();
    current = _current;
    other = _other;
  }

  void* Set(void* cmd, GLenum _current, GLenum _other) {
    static_cast<ValueType*>(cmd)->Init(_current, _other);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t current;
  uint32_t other;
};

static_assert(sizeof(LoseContextCHROMIUM) == 12,
              "size of LoseContextCHROMIUM should be 12");
static_assert(offsetof(LoseContextCHROMIUM, header) == 0,
              "offset of LoseContextCHROMIUM header should be 0");
static_assert(offsetof(LoseContextCHROMIUM, current) == 4,
              "offset of LoseContextCHROMIUM current should be 4");
static_assert(offsetof(LoseContextCHROMIUM, other) == 8,
              "offset of LoseContextCHROMIUM other should be 8");

struct WaitSyncTokenCHROMIUM {
  typedef WaitSyncTokenCHROMIUM ValueType;
  static const CommandId kCmdId = kWaitSyncTokenCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLint _namespace_id,
            GLuint64 _command_buffer_id,
            GLuint64 _release_count) {
    SetHeader();
    namespace_id = _namespace_id;
    gles2::GLES2Util::MapUint64ToTwoUint32(
        static_cast<uint64_t>(_command_buffer_id), &command_buffer_id_0,
        &command_buffer_id_1);
    gles2::GLES2Util::MapUint64ToTwoUint32(
        static_cast<uint64_t>(_release_count), &release_count_0,
        &release_count_1);
  }

  void* Set(void* cmd,
            GLint _namespace_id,
            GLuint64 _command_buffer_id,
            GLuint64 _release_count) {
    static_cast<ValueType*>(cmd)->Init(_namespace_id, _command_buffer_id,
                                       _release_count);
    return NextCmdAddress<ValueType>(cmd);
  }

  GLuint64 command_buffer_id() const volatile {
    return static_cast<GLuint64>(gles2::GLES2Util::MapTwoUint32ToUint64(
        command_buffer_id_0, command_buffer_id_1));
  }

  GLuint64 release_count() const volatile {
    return static_cast<GLuint64>(gles2::GLES2Util::MapTwoUint32ToUint64(
        release_count_0, release_count_1));
  }

  gpu::CommandHeader header;
  int32_t namespace_id;
  uint32_t command_buffer_id_0;
  uint32_t command_buffer_id_1;
  uint32_t release_count_0;
  uint32_t release_count_1;
};

static_assert(sizeof(WaitSyncTokenCHROMIUM) == 24,
              "size of WaitSyncTokenCHROMIUM should be 24");
static_assert(offsetof(WaitSyncTokenCHROMIUM, header) == 0,
              "offset of WaitSyncTokenCHROMIUM header should be 0");
static_assert(offsetof(WaitSyncTokenCHROMIUM, namespace_id) == 4,
              "offset of WaitSyncTokenCHROMIUM namespace_id should be 4");
static_assert(
    offsetof(WaitSyncTokenCHROMIUM, command_buffer_id_0) == 8,
    "offset of WaitSyncTokenCHROMIUM command_buffer_id_0 should be 8");
static_assert(
    offsetof(WaitSyncTokenCHROMIUM, command_buffer_id_1) == 12,
    "offset of WaitSyncTokenCHROMIUM command_buffer_id_1 should be 12");
static_assert(offsetof(WaitSyncTokenCHROMIUM, release_count_0) == 16,
              "offset of WaitSyncTokenCHROMIUM release_count_0 should be 16");
static_assert(offsetof(WaitSyncTokenCHROMIUM, release_count_1) == 20,
              "offset of WaitSyncTokenCHROMIUM release_count_1 should be 20");

struct InitializeDiscardableTextureCHROMIUM {
  typedef InitializeDiscardableTextureCHROMIUM ValueType;
  static const CommandId kCmdId = kInitializeDiscardableTextureCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture_id, uint32_t _shm_id, uint32_t _shm_offset) {
    SetHeader();
    texture_id = _texture_id;
    shm_id = _shm_id;
    shm_offset = _shm_offset;
  }

  void* Set(void* cmd,
            GLuint _texture_id,
            uint32_t _shm_id,
            uint32_t _shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_texture_id, _shm_id, _shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture_id;
  uint32_t shm_id;
  uint32_t shm_offset;
};

static_assert(sizeof(InitializeDiscardableTextureCHROMIUM) == 16,
              "size of InitializeDiscardableTextureCHROMIUM should be 16");
static_assert(
    offsetof(InitializeDiscardableTextureCHROMIUM, header) == 0,
    "offset of InitializeDiscardableTextureCHROMIUM header should be 0");
static_assert(
    offsetof(InitializeDiscardableTextureCHROMIUM, texture_id) == 4,
    "offset of InitializeDiscardableTextureCHROMIUM texture_id should be 4");
static_assert(
    offsetof(InitializeDiscardableTextureCHROMIUM, shm_id) == 8,
    "offset of InitializeDiscardableTextureCHROMIUM shm_id should be 8");
static_assert(
    offsetof(InitializeDiscardableTextureCHROMIUM, shm_offset) == 12,
    "offset of InitializeDiscardableTextureCHROMIUM shm_offset should be 12");

struct UnlockDiscardableTextureCHROMIUM {
  typedef UnlockDiscardableTextureCHROMIUM ValueType;
  static const CommandId kCmdId = kUnlockDiscardableTextureCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture_id) {
    SetHeader();
    texture_id = _texture_id;
  }

  void* Set(void* cmd, GLuint _texture_id) {
    static_cast<ValueType*>(cmd)->Init(_texture_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture_id;
};

static_assert(sizeof(UnlockDiscardableTextureCHROMIUM) == 8,
              "size of UnlockDiscardableTextureCHROMIUM should be 8");
static_assert(offsetof(UnlockDiscardableTextureCHROMIUM, header) == 0,
              "offset of UnlockDiscardableTextureCHROMIUM header should be 0");
static_assert(
    offsetof(UnlockDiscardableTextureCHROMIUM, texture_id) == 4,
    "offset of UnlockDiscardableTextureCHROMIUM texture_id should be 4");

struct LockDiscardableTextureCHROMIUM {
  typedef LockDiscardableTextureCHROMIUM ValueType;
  static const CommandId kCmdId = kLockDiscardableTextureCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture_id) {
    SetHeader();
    texture_id = _texture_id;
  }

  void* Set(void* cmd, GLuint _texture_id) {
    static_cast<ValueType*>(cmd)->Init(_texture_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture_id;
};

static_assert(sizeof(LockDiscardableTextureCHROMIUM) == 8,
              "size of LockDiscardableTextureCHROMIUM should be 8");
static_assert(offsetof(LockDiscardableTextureCHROMIUM, header) == 0,
              "offset of LockDiscardableTextureCHROMIUM header should be 0");
static_assert(
    offsetof(LockDiscardableTextureCHROMIUM, texture_id) == 4,
    "offset of LockDiscardableTextureCHROMIUM texture_id should be 4");

struct BeginRasterCHROMIUM {
  typedef BeginRasterCHROMIUM ValueType;
  static const CommandId kCmdId = kBeginRasterCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _texture_id,
            GLuint _sk_color,
            GLuint _msaa_sample_count,
            GLboolean _can_use_lcd_text,
            GLboolean _use_distance_field_text,
            GLint _pixel_config) {
    SetHeader();
    texture_id = _texture_id;
    sk_color = _sk_color;
    msaa_sample_count = _msaa_sample_count;
    can_use_lcd_text = _can_use_lcd_text;
    use_distance_field_text = _use_distance_field_text;
    pixel_config = _pixel_config;
  }

  void* Set(void* cmd,
            GLuint _texture_id,
            GLuint _sk_color,
            GLuint _msaa_sample_count,
            GLboolean _can_use_lcd_text,
            GLboolean _use_distance_field_text,
            GLint _pixel_config) {
    static_cast<ValueType*>(cmd)->Init(_texture_id, _sk_color,
                                       _msaa_sample_count, _can_use_lcd_text,
                                       _use_distance_field_text, _pixel_config);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t texture_id;
  uint32_t sk_color;
  uint32_t msaa_sample_count;
  uint32_t can_use_lcd_text;
  uint32_t use_distance_field_text;
  int32_t pixel_config;
};

static_assert(sizeof(BeginRasterCHROMIUM) == 28,
              "size of BeginRasterCHROMIUM should be 28");
static_assert(offsetof(BeginRasterCHROMIUM, header) == 0,
              "offset of BeginRasterCHROMIUM header should be 0");
static_assert(offsetof(BeginRasterCHROMIUM, texture_id) == 4,
              "offset of BeginRasterCHROMIUM texture_id should be 4");
static_assert(offsetof(BeginRasterCHROMIUM, sk_color) == 8,
              "offset of BeginRasterCHROMIUM sk_color should be 8");
static_assert(offsetof(BeginRasterCHROMIUM, msaa_sample_count) == 12,
              "offset of BeginRasterCHROMIUM msaa_sample_count should be 12");
static_assert(offsetof(BeginRasterCHROMIUM, can_use_lcd_text) == 16,
              "offset of BeginRasterCHROMIUM can_use_lcd_text should be 16");
static_assert(
    offsetof(BeginRasterCHROMIUM, use_distance_field_text) == 20,
    "offset of BeginRasterCHROMIUM use_distance_field_text should be 20");
static_assert(offsetof(BeginRasterCHROMIUM, pixel_config) == 24,
              "offset of BeginRasterCHROMIUM pixel_config should be 24");

struct RasterCHROMIUM {
  typedef RasterCHROMIUM ValueType;
  static const CommandId kCmdId = kRasterCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLsizeiptr _size,
            uint32_t _list_shm_id,
            uint32_t _list_shm_offset) {
    SetHeader();
    size = _size;
    list_shm_id = _list_shm_id;
    list_shm_offset = _list_shm_offset;
  }

  void* Set(void* cmd,
            GLsizeiptr _size,
            uint32_t _list_shm_id,
            uint32_t _list_shm_offset) {
    static_cast<ValueType*>(cmd)->Init(_size, _list_shm_id, _list_shm_offset);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  int32_t size;
  uint32_t list_shm_id;
  uint32_t list_shm_offset;
};

static_assert(sizeof(RasterCHROMIUM) == 16,
              "size of RasterCHROMIUM should be 16");
static_assert(offsetof(RasterCHROMIUM, header) == 0,
              "offset of RasterCHROMIUM header should be 0");
static_assert(offsetof(RasterCHROMIUM, size) == 4,
              "offset of RasterCHROMIUM size should be 4");
static_assert(offsetof(RasterCHROMIUM, list_shm_id) == 8,
              "offset of RasterCHROMIUM list_shm_id should be 8");
static_assert(offsetof(RasterCHROMIUM, list_shm_offset) == 12,
              "offset of RasterCHROMIUM list_shm_offset should be 12");

struct EndRasterCHROMIUM {
  typedef EndRasterCHROMIUM ValueType;
  static const CommandId kCmdId = kEndRasterCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init() { SetHeader(); }

  void* Set(void* cmd) {
    static_cast<ValueType*>(cmd)->Init();
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
};

static_assert(sizeof(EndRasterCHROMIUM) == 4,
              "size of EndRasterCHROMIUM should be 4");
static_assert(offsetof(EndRasterCHROMIUM, header) == 0,
              "offset of EndRasterCHROMIUM header should be 0");

struct CreateTransferCacheEntryINTERNAL {
  typedef CreateTransferCacheEntryINTERNAL ValueType;
  static const CommandId kCmdId = kCreateTransferCacheEntryINTERNAL;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _entry_type,
            GLuint _entry_id,
            GLuint _handle_shm_id,
            GLuint _handle_shm_offset,
            GLuint _data_shm_id,
            GLuint _data_shm_offset,
            GLuint _data_size) {
    SetHeader();
    entry_type = _entry_type;
    entry_id = _entry_id;
    handle_shm_id = _handle_shm_id;
    handle_shm_offset = _handle_shm_offset;
    data_shm_id = _data_shm_id;
    data_shm_offset = _data_shm_offset;
    data_size = _data_size;
  }

  void* Set(void* cmd,
            GLuint _entry_type,
            GLuint _entry_id,
            GLuint _handle_shm_id,
            GLuint _handle_shm_offset,
            GLuint _data_shm_id,
            GLuint _data_shm_offset,
            GLuint _data_size) {
    static_cast<ValueType*>(cmd)->Init(_entry_type, _entry_id, _handle_shm_id,
                                       _handle_shm_offset, _data_shm_id,
                                       _data_shm_offset, _data_size);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t entry_type;
  uint32_t entry_id;
  uint32_t handle_shm_id;
  uint32_t handle_shm_offset;
  uint32_t data_shm_id;
  uint32_t data_shm_offset;
  uint32_t data_size;
};

static_assert(sizeof(CreateTransferCacheEntryINTERNAL) == 32,
              "size of CreateTransferCacheEntryINTERNAL should be 32");
static_assert(offsetof(CreateTransferCacheEntryINTERNAL, header) == 0,
              "offset of CreateTransferCacheEntryINTERNAL header should be 0");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, entry_type) == 4,
    "offset of CreateTransferCacheEntryINTERNAL entry_type should be 4");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, entry_id) == 8,
    "offset of CreateTransferCacheEntryINTERNAL entry_id should be 8");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, handle_shm_id) == 12,
    "offset of CreateTransferCacheEntryINTERNAL handle_shm_id should be 12");
static_assert(offsetof(CreateTransferCacheEntryINTERNAL, handle_shm_offset) ==
                  16,
              "offset of CreateTransferCacheEntryINTERNAL handle_shm_offset "
              "should be 16");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, data_shm_id) == 20,
    "offset of CreateTransferCacheEntryINTERNAL data_shm_id should be 20");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, data_shm_offset) == 24,
    "offset of CreateTransferCacheEntryINTERNAL data_shm_offset should be 24");
static_assert(
    offsetof(CreateTransferCacheEntryINTERNAL, data_size) == 28,
    "offset of CreateTransferCacheEntryINTERNAL data_size should be 28");

struct DeleteTransferCacheEntryINTERNAL {
  typedef DeleteTransferCacheEntryINTERNAL ValueType;
  static const CommandId kCmdId = kDeleteTransferCacheEntryINTERNAL;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _entry_type, GLuint _entry_id) {
    SetHeader();
    entry_type = _entry_type;
    entry_id = _entry_id;
  }

  void* Set(void* cmd, GLuint _entry_type, GLuint _entry_id) {
    static_cast<ValueType*>(cmd)->Init(_entry_type, _entry_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t entry_type;
  uint32_t entry_id;
};

static_assert(sizeof(DeleteTransferCacheEntryINTERNAL) == 12,
              "size of DeleteTransferCacheEntryINTERNAL should be 12");
static_assert(offsetof(DeleteTransferCacheEntryINTERNAL, header) == 0,
              "offset of DeleteTransferCacheEntryINTERNAL header should be 0");
static_assert(
    offsetof(DeleteTransferCacheEntryINTERNAL, entry_type) == 4,
    "offset of DeleteTransferCacheEntryINTERNAL entry_type should be 4");
static_assert(
    offsetof(DeleteTransferCacheEntryINTERNAL, entry_id) == 8,
    "offset of DeleteTransferCacheEntryINTERNAL entry_id should be 8");

struct UnlockTransferCacheEntryINTERNAL {
  typedef UnlockTransferCacheEntryINTERNAL ValueType;
  static const CommandId kCmdId = kUnlockTransferCacheEntryINTERNAL;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLuint _entry_type, GLuint _entry_id) {
    SetHeader();
    entry_type = _entry_type;
    entry_id = _entry_id;
  }

  void* Set(void* cmd, GLuint _entry_type, GLuint _entry_id) {
    static_cast<ValueType*>(cmd)->Init(_entry_type, _entry_id);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t entry_type;
  uint32_t entry_id;
};

static_assert(sizeof(UnlockTransferCacheEntryINTERNAL) == 12,
              "size of UnlockTransferCacheEntryINTERNAL should be 12");
static_assert(offsetof(UnlockTransferCacheEntryINTERNAL, header) == 0,
              "offset of UnlockTransferCacheEntryINTERNAL header should be 0");
static_assert(
    offsetof(UnlockTransferCacheEntryINTERNAL, entry_type) == 4,
    "offset of UnlockTransferCacheEntryINTERNAL entry_type should be 4");
static_assert(
    offsetof(UnlockTransferCacheEntryINTERNAL, entry_id) == 8,
    "offset of UnlockTransferCacheEntryINTERNAL entry_id should be 8");

struct TexStorage2DImageCHROMIUM {
  typedef TexStorage2DImageCHROMIUM ValueType;
  static const CommandId kCmdId = kTexStorage2DImageCHROMIUM;
  static const cmd::ArgFlags kArgFlags = cmd::kFixed;
  static const uint8_t cmd_flags = CMD_FLAG_SET_TRACE_LEVEL(3);

  static uint32_t ComputeSize() {
    return static_cast<uint32_t>(sizeof(ValueType));  // NOLINT
  }

  void SetHeader() { header.SetCmd<ValueType>(); }

  void Init(GLenum _target,
            GLenum _internalFormat,
            GLsizei _width,
            GLsizei _height) {
    SetHeader();
    target = _target;
    internalFormat = _internalFormat;
    width = _width;
    height = _height;
  }

  void* Set(void* cmd,
            GLenum _target,
            GLenum _internalFormat,
            GLsizei _width,
            GLsizei _height) {
    static_cast<ValueType*>(cmd)->Init(_target, _internalFormat, _width,
                                       _height);
    return NextCmdAddress<ValueType>(cmd);
  }

  gpu::CommandHeader header;
  uint32_t target;
  uint32_t internalFormat;
  int32_t width;
  int32_t height;
  static const uint32_t bufferUsage = GL_SCANOUT_CHROMIUM;
};

static_assert(sizeof(TexStorage2DImageCHROMIUM) == 20,
              "size of TexStorage2DImageCHROMIUM should be 20");
static_assert(offsetof(TexStorage2DImageCHROMIUM, header) == 0,
              "offset of TexStorage2DImageCHROMIUM header should be 0");
static_assert(offsetof(TexStorage2DImageCHROMIUM, target) == 4,
              "offset of TexStorage2DImageCHROMIUM target should be 4");
static_assert(offsetof(TexStorage2DImageCHROMIUM, internalFormat) == 8,
              "offset of TexStorage2DImageCHROMIUM internalFormat should be 8");
static_assert(offsetof(TexStorage2DImageCHROMIUM, width) == 12,
              "offset of TexStorage2DImageCHROMIUM width should be 12");
static_assert(offsetof(TexStorage2DImageCHROMIUM, height) == 16,
              "offset of TexStorage2DImageCHROMIUM height should be 16");

#endif  // GPU_COMMAND_BUFFER_COMMON_RASTER_CMD_FORMAT_AUTOGEN_H_
