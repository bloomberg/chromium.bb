// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is here so other GLES2 related files can have a common set of
// includes where appropriate.

#ifndef GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_UTILS_H_
#define GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/numerics/safe_math.h"
#include "gpu/command_buffer/common/gles2_utils_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gpu_preference.h"

namespace gpu {
namespace gles2 {

// Does a multiply and checks for overflow.  If the multiply did not overflow
// returns true.

// Multiplies 2 32 bit unsigned numbers checking for overflow.
// If there was no overflow returns true.
inline bool SafeMultiplyUint32(uint32_t a, uint32_t b, uint32_t* dst) {
  DCHECK(dst);
  base::CheckedNumeric<uint32_t> checked = a;
  checked *= b;
  *dst = checked.ValueOrDefault(0);
  return checked.IsValid();
}

// Does an add checking for overflow.  If there was no overflow returns true.
inline bool SafeAddUint32(uint32_t a, uint32_t b, uint32_t* dst) {
  DCHECK(dst);
  base::CheckedNumeric<uint32_t> checked = a;
  checked += b;
  *dst = checked.ValueOrDefault(0);
  return checked.IsValid();
}

// Does an add checking for overflow.  If there was no overflow returns true.
inline bool SafeAddInt32(int32_t a, int32_t b, int32_t* dst) {
  DCHECK(dst);
  base::CheckedNumeric<int32_t> checked = a;
  checked += b;
  *dst = checked.ValueOrDefault(0);
  return checked.IsValid();
}

// Returns the address of the first byte after a struct.
template <typename T>
const volatile void* AddressAfterStruct(const volatile T& pod) {
  return reinterpret_cast<const volatile uint8_t*>(&pod) + sizeof(pod);
}

// Returns the address of the frst byte after the struct or NULL if size >
// immediate_data_size.
template <typename RETURN_TYPE, typename COMMAND_TYPE>
RETURN_TYPE GetImmediateDataAs(const volatile COMMAND_TYPE& pod,
                               uint32_t size,
                               uint32_t immediate_data_size) {
  return (size <= immediate_data_size)
             ? static_cast<RETURN_TYPE>(
                   const_cast<volatile void*>(AddressAfterStruct(pod)))
             : NULL;
}

struct GLES2_UTILS_EXPORT PixelStoreParams {
  PixelStoreParams()
      : alignment(4),
        row_length(0),
        image_height(0),
        skip_pixels(0),
        skip_rows(0),
        skip_images(0) {
  }

  int32_t alignment;
  int32_t row_length;
  int32_t image_height;
  int32_t skip_pixels;
  int32_t skip_rows;
  int32_t skip_images;
};

// Utilties for GLES2 support.
class GLES2_UTILS_EXPORT GLES2Util {
 public:
  static const int kNumFaces = 6;

  // Bits returned by GetChannelsForFormat
  enum ChannelBits {
    kRed = 0x1,
    kGreen = 0x2,
    kBlue = 0x4,
    kAlpha = 0x8,
    kDepth = 0x10000,
    kStencil = 0x20000,

    kRGB = kRed | kGreen | kBlue,
    kRGBA = kRGB | kAlpha
  };

  struct EnumToString {
    uint32_t value;
    const char* name;
  };

  GLES2Util()
      : num_compressed_texture_formats_(0),
        num_shader_binary_formats_(0) {
  }

  int num_compressed_texture_formats() const {
    return num_compressed_texture_formats_;
  }

  void set_num_compressed_texture_formats(int num_compressed_texture_formats) {
    num_compressed_texture_formats_ = num_compressed_texture_formats;
  }

  int num_shader_binary_formats() const {
    return num_shader_binary_formats_;
  }

  void set_num_shader_binary_formats(int num_shader_binary_formats) {
    num_shader_binary_formats_ = num_shader_binary_formats;
  }

  // Gets the number of values a particular id will return when a glGet
  // function is called. If 0 is returned the id is invalid.
  int GLGetNumValuesReturned(int id) const;

  static int ElementsPerGroup(int format, int type);
  // Computes the size of a single group of elements from a format and type pair
  static uint32_t ComputeImageGroupSize(int format, int type);

  // Computes the size of an image row including alignment padding
  static bool ComputeImagePaddedRowSize(
      int width, int format, int type, int alignment,
      uint32_t* padded_row_size);

  // Computes the size of image data for TexImage2D and TexSubImage2D.
  // Optionally the unpadded and padded row sizes can be returned.
  static bool ComputeImageDataSizes(
      int width, int height, int depth, int format, int type,
      int alignment, uint32_t* size, uint32_t* opt_unpadded_row_size,
      uint32_t* opt_padded_row_size);

  // Similar to the above function, but taking into consideration all ES3
  // pixel pack/unpack parameters.
  // Optionally the skipped bytes in the beginning can be returned.
  // Note the returned |size| does NOT include |skip_size|.
  // TODO(zmo): merging ComputeImageDataSize and ComputeImageDataSizeES3.
  static bool ComputeImageDataSizesES3(
      int width, int height, int depth, int format, int type,
      const PixelStoreParams& params,
      uint32_t* size, uint32_t* opt_unpadded_row_size,
      uint32_t* opt_padded_row_size, uint32_t* opt_skip_size,
      uint32_t* opt_padding);

  static size_t RenderbufferBytesPerPixel(int format);

  // Return the element's number of bytes.
  // For example, GL_FLOAT_MAT3 returns sizeof(GLfloat).
  static uint32_t GetElementSizeForUniformType(int type);
  // Return the number of elements.
  // For example, GL_FLOAT_MAT3 returns 9.
  static uint32_t GetElementCountForUniformType(int type);

  static size_t GetGLTypeSizeForTextures(uint32_t type);

  static size_t GetGLTypeSizeForBuffers(uint32_t type);

  static size_t GetGroupSizeForBufferType(uint32_t count, uint32_t type);

  static size_t GetGLTypeSizeForPathCoordType(uint32_t type);

  static uint32_t GLErrorToErrorBit(uint32_t gl_error);

  static size_t GetComponentCountForGLTransformType(uint32_t type);
  static size_t GetGLTypeSizeForGLPathNameType(uint32_t type);

  static size_t GetCoefficientCountForGLPathFragmentInputGenMode(
      uint32_t gen_mode);

  static uint32_t GLErrorBitToGLError(uint32_t error_bit);

  static uint32_t IndexToGLFaceTarget(int index);

  static size_t GLTargetToFaceIndex(uint32_t target);
  static uint32_t GLFaceTargetToTextureTarget(uint32_t target);

  static uint32_t GetGLReadPixelsImplementationFormat(uint32_t internal_format,
                                                      uint32_t texture_type,
                                                      bool supports_bgra);

  static uint32_t GetGLReadPixelsImplementationType(
      uint32_t internal_format, uint32_t texture_type);

  // Returns a bitmask for the channels the given format supports.
  // See ChannelBits.
  static uint32_t GetChannelsForFormat(int format);

  // Returns a bitmask for the channels the given attachment type needs.
  static uint32_t GetChannelsNeededForAttachmentType(
      int type, uint32_t max_color_attachments);

  // Return true if value is neither a power of two nor zero.
  static bool IsNPOT(uint32_t value) {
    return (value & (value - 1)) != 0;
  }

  // Return true if value is a power of two or zero.
  static bool IsPOT(uint32_t value) {
    return (value & (value - 1)) == 0;
  }

  static std::string GetStringEnum(uint32_t value);
  static std::string GetStringBool(uint32_t value);
  static std::string GetStringError(uint32_t value);

  static size_t CalcClearBufferivDataCount(int buffer);
  static size_t CalcClearBufferfvDataCount(int buffer);

  static void MapUint64ToTwoUint32(
      uint64_t v64, uint32_t* v32_0, uint32_t* v32_1);
  static uint64_t MapTwoUint32ToUint64(uint32_t v32_0, uint32_t v32_1);

  static uint32_t MapBufferTargetToBindingEnum(uint32_t target);

  static bool IsUnsignedIntegerFormat(uint32_t internal_format);
  static bool IsSignedIntegerFormat(uint32_t internal_format);
  static bool IsIntegerFormat(uint32_t internal_format);
  static bool IsFloatFormat(uint32_t internal_format);
  static uint32_t ConvertToSizedFormat(uint32_t format, uint32_t type);
  static bool IsSizedColorFormat(uint32_t internal_format);

  // Infer color encoding from internalformat
  static int GetColorEncodingFromInternalFormat(uint32_t internalformat);

  static void GetColorFormatComponentSizes(
      uint32_t internal_format, uint32_t type, int* r, int* g, int* b, int* a);

  // Computes the data size for certain gl commands like glUniform.
  template <typename VALUE_TYPE, unsigned int ELEMENTS_PER_UNIT>
  static bool ComputeDataSize(uint32_t count, uint32_t* dst) {
    constexpr uint32_t element_size = sizeof(VALUE_TYPE) * ELEMENTS_PER_UNIT;
    return base::CheckMul(count, element_size).AssignIfValid(dst);
  }

#include "gpu/command_buffer/common/gles2_cmd_utils_autogen.h"

 private:
  static std::string GetQualifiedEnumString(
      const EnumToString* table, size_t count, uint32_t value);

  static bool ComputeImageRowSizeHelper(int width,
                                        uint32_t bytes_per_group,
                                        int alignment,
                                        uint32_t* rt_unpadded_row_size,
                                        uint32_t* rt_padded_row_size,
                                        uint32_t* rt_padding);

  static const EnumToString* const enum_to_string_table_;
  static const size_t enum_to_string_table_len_;

  int num_compressed_texture_formats_;
  int num_shader_binary_formats_;
};

class GLES2_UTILS_EXPORT GLSLArrayName {
 public:
  explicit GLSLArrayName(const std::string& name);

  // Returns true if the string is an array reference.
  bool IsArrayName() const { return element_index_ >= 0; }
  // Returns the name with the possible last array index specifier removed.
  std::string base_name() const {
    DCHECK(IsArrayName());
    return base_name_;
  }
  // Returns the element index of a name which references an array element.
  int element_index() const {
    DCHECK(IsArrayName());
    return element_index_;
  }

 private:
  std::string base_name_;
  int element_index_;
  DISALLOW_COPY_AND_ASSIGN(GLSLArrayName);
};

enum ContextType {
  CONTEXT_TYPE_WEBGL1,
  CONTEXT_TYPE_WEBGL2,
  CONTEXT_TYPE_OPENGLES2,
  CONTEXT_TYPE_OPENGLES3,
  CONTEXT_TYPE_LAST = CONTEXT_TYPE_OPENGLES3
};
GLES2_UTILS_EXPORT bool IsWebGLContextType(ContextType context_type);
GLES2_UTILS_EXPORT bool IsWebGL1OrES2ContextType(ContextType context_type);
GLES2_UTILS_EXPORT bool IsWebGL2OrES3ContextType(ContextType context_type);

enum ColorSpace {
  COLOR_SPACE_UNSPECIFIED,
  COLOR_SPACE_SRGB,
  COLOR_SPACE_DISPLAY_P3,
  COLOR_SPACE_LAST = COLOR_SPACE_DISPLAY_P3
};

struct GLES2_UTILS_EXPORT ContextCreationAttribHelper {
  ContextCreationAttribHelper();
  ContextCreationAttribHelper(const ContextCreationAttribHelper& other);

  bool Parse(const std::vector<int32_t>& attribs);

  gfx::Size offscreen_framebuffer_size;
  gl::GpuPreference gpu_preference;
  // -1 if invalid or unspecified.
  int32_t alpha_size;
  int32_t blue_size;
  int32_t green_size;
  int32_t red_size;
  int32_t depth_size;
  int32_t stencil_size;
  int32_t samples;
  int32_t sample_buffers;
  bool buffer_preserved;
  bool bind_generates_resource;
  bool fail_if_major_perf_caveat;
  bool lose_context_when_out_of_memory;
  bool should_use_native_gmb_for_backbuffer;
  bool own_offscreen_surface;
  bool single_buffer;

  ContextType context_type;
  ColorSpace color_space = COLOR_SPACE_UNSPECIFIED;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_UTILS_H_
