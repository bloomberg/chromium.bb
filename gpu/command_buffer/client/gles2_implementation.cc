// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class to emulate GLES2 over command buffers.

#include "../client/gles2_implementation.h"

#include <map>
#include <set>
#include <queue>
#include <GLES2/gl2ext.h>
#include "../client/mapped_memory.h"
#include "../client/program_info_manager.h"
#include "../client/query_tracker.h"
#include "../client/transfer_buffer.h"
#include "../common/gles2_cmd_utils.h"
#include "../common/trace_event.h"

#if defined(__native_client__) && !defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
#define GLES2_SUPPORT_CLIENT_SIDE_ARRAYS
#endif

#if defined(GPU_CLIENT_DEBUG)
#include "ui/gl/gl_switches.h"
#include "base/command_line.h"
#endif

namespace gpu {
namespace gles2 {

// A 32-bit and 64-bit compatible way of converting a pointer to a GLuint.
static GLuint ToGLuint(const void* ptr) {
  return static_cast<GLuint>(reinterpret_cast<size_t>(ptr));
}

static GLsizei RoundUpToMultipleOf4(GLsizei size) {
  return (size + 3) & ~3;
}

// This class tracks VertexAttribPointers and helps emulate client side buffers.
//
// The way client side buffers work is we shadow all the Vertex Attribs so we
// know which ones are pointing to client side buffers.
//
// At Draw time, for any attribs pointing to client side buffers we copy them
// to a special VBO and reset the actual vertex attrib pointers to point to this
// VBO.
//
// This also means we have to catch calls to query those values so that when
// an attrib is a client side buffer we pass the info back the user expects.
class ClientSideBufferHelper {
 public:
  // Info about Vertex Attributes. This is used to track what the user currently
  // has bound on each Vertex Attribute so we can simulate client side buffers
  // at glDrawXXX time.
  class VertexAttribInfo {
   public:
    VertexAttribInfo()
        : enabled_(false),
          buffer_id_(0),
          size_(4),
          type_(GL_FLOAT),
          normalized_(GL_FALSE),
          pointer_(NULL),
          gl_stride_(0),
          divisor_(0) {
    }

    bool enabled() const {
      return enabled_;
    }

    void set_enabled(bool enabled) {
      enabled_ = enabled;
    }

    GLuint buffer_id() const {
      return buffer_id_;
    }

    GLenum type() const {
      return type_;
    }

    GLint size() const {
      return size_;
    }

    GLsizei stride() const {
      return gl_stride_;
    }

    GLboolean normalized() const {
      return normalized_;
    }

    const GLvoid* pointer() const {
      return pointer_;
    }

    bool IsClientSide() const {
      return buffer_id_ == 0;
    }

    GLuint divisor() const {
      return divisor_;
    }

    void SetInfo(
        GLuint buffer_id,
        GLint size,
        GLenum type,
        GLboolean normalized,
        GLsizei gl_stride,
        const GLvoid* pointer) {
      buffer_id_ = buffer_id;
      size_ = size;
      type_ = type;
      normalized_ = normalized;
      gl_stride_ = gl_stride;
      pointer_ = pointer;
    }

    void SetDivisor(GLuint divisor) {
      divisor_ = divisor;
    }

   private:
    // Whether or not this attribute is enabled.
    bool enabled_;

    // The id of the buffer. 0 = client side buffer.
    GLuint buffer_id_;

    // Number of components (1, 2, 3, 4).
    GLint size_;

    // GL_BYTE, GL_FLOAT, etc. See glVertexAttribPointer.
    GLenum type_;

    // GL_TRUE or GL_FALSE
    GLboolean normalized_;

    // The pointer/offset into the buffer.
    const GLvoid* pointer_;

    // The stride that will be used to access the buffer. This is the bogus GL
    // stride where 0 = compute the stride based on size and type.
    GLsizei gl_stride_;

    // Divisor, for geometry instancing.
    GLuint divisor_;
  };

  ClientSideBufferHelper(GLuint max_vertex_attribs,
                         GLuint array_buffer_id,
                         GLuint element_array_buffer_id)
      : max_vertex_attribs_(max_vertex_attribs),
        num_client_side_pointers_enabled_(0),
        array_buffer_id_(array_buffer_id),
        array_buffer_size_(0),
        array_buffer_offset_(0),
        element_array_buffer_id_(element_array_buffer_id),
        element_array_buffer_size_(0),
        collection_buffer_size_(0) {
    vertex_attrib_infos_.reset(new VertexAttribInfo[max_vertex_attribs]);
  }

  bool HaveEnabledClientSideBuffers() const {
    return num_client_side_pointers_enabled_ > 0;
  }

  void SetAttribEnable(GLuint index, bool enabled) {
    if (index < max_vertex_attribs_) {
      VertexAttribInfo& info = vertex_attrib_infos_[index];
      if (info.enabled() != enabled) {
        if (info.IsClientSide()) {
          num_client_side_pointers_enabled_ += enabled ? 1 : -1;
        }
        info.set_enabled(enabled);
      }
    }
  }

  void SetAttribPointer(
    GLuint buffer_id,
    GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
    const void* ptr) {
    if (index < max_vertex_attribs_) {
      VertexAttribInfo& info = vertex_attrib_infos_[index];
      if (info.IsClientSide() && info.enabled()) {
        --num_client_side_pointers_enabled_;
      }

      info.SetInfo(buffer_id, size, type, normalized, stride, ptr);

      if (info.IsClientSide() && info.enabled()) {
        ++num_client_side_pointers_enabled_;
      }
    }
  }

  void SetAttribDivisor(GLuint index, GLuint divisor) {
    if (index < max_vertex_attribs_) {
      VertexAttribInfo& info = vertex_attrib_infos_[index];

      info.SetDivisor(divisor);
    }
  }

  // Gets the Attrib pointer for an attrib but only if it's a client side
  // pointer. Returns true if it got the pointer.
  bool GetAttribPointer(GLuint index, GLenum pname, void** ptr) const {
    const VertexAttribInfo* info = GetAttribInfo(index);
    if (info && pname == GL_VERTEX_ATTRIB_ARRAY_POINTER) {
      *ptr = const_cast<void*>(info->pointer());
      return true;
    }
    return false;
  }

  // Gets an attrib info if it's in range and it's client side.
  const VertexAttribInfo* GetAttribInfo(GLuint index) const {
    if (index < max_vertex_attribs_) {
      VertexAttribInfo* info = &vertex_attrib_infos_[index];
      if (info->IsClientSide()) {
        return info;
      }
    }
    return NULL;
  }

  // Collects the data into the collection buffer and returns the number of
  // bytes collected.
  GLsizei CollectData(const void* data,
                      GLsizei bytes_per_element,
                      GLsizei real_stride,
                      GLsizei num_elements) {
    GLsizei bytes_needed = bytes_per_element * num_elements;
    if (collection_buffer_size_ < bytes_needed) {
      collection_buffer_.reset(new int8[bytes_needed]);
      collection_buffer_size_ = bytes_needed;
    }
    const int8* src = static_cast<const int8*>(data);
    int8* dst = collection_buffer_.get();
    int8* end = dst + bytes_per_element * num_elements;
    for (; dst < end; src += real_stride, dst += bytes_per_element) {
      memcpy(dst, src, bytes_per_element);
    }
    return bytes_needed;
  }

  // Returns true if buffers were setup.
  void SetupSimulatedClientSideBuffers(
      GLES2Implementation* gl,
      GLES2CmdHelper* gl_helper,
      GLsizei num_elements,
      GLsizei primcount) {
    GLsizei total_size = 0;
    // Compute the size of the buffer we need.
    for (GLuint ii = 0; ii < max_vertex_attribs_; ++ii) {
      VertexAttribInfo& info = vertex_attrib_infos_[ii];
      if (info.IsClientSide() && info.enabled()) {
        size_t bytes_per_element =
            GLES2Util::GetGLTypeSizeForTexturesAndBuffers(info.type()) *
            info.size();
        GLsizei elements = (primcount && info.divisor() > 0) ?
            ((primcount - 1) / info.divisor() + 1) : num_elements;
        total_size += RoundUpToMultipleOf4(
            bytes_per_element * elements);
      }
    }
    gl_helper->BindBuffer(GL_ARRAY_BUFFER, array_buffer_id_);
    array_buffer_offset_ = 0;
    if (total_size > array_buffer_size_) {
      gl->BufferDataHelper(GL_ARRAY_BUFFER, total_size, NULL, GL_DYNAMIC_DRAW);
      array_buffer_size_ = total_size;
    }
    for (GLuint ii = 0; ii < max_vertex_attribs_; ++ii) {
      VertexAttribInfo& info = vertex_attrib_infos_[ii];
      if (info.IsClientSide() && info.enabled()) {
        size_t bytes_per_element =
            GLES2Util::GetGLTypeSizeForTexturesAndBuffers(info.type()) *
            info.size();
        GLsizei real_stride = info.stride() ?
            info.stride() : static_cast<GLsizei>(bytes_per_element);
        GLsizei elements = (primcount && info.divisor() > 0) ?
            ((primcount - 1) / info.divisor() + 1) : num_elements;
        GLsizei bytes_collected = CollectData(
            info.pointer(), bytes_per_element, real_stride, elements);
        gl->BufferSubDataHelper(
            GL_ARRAY_BUFFER, array_buffer_offset_, bytes_collected,
            collection_buffer_.get());
        gl_helper->VertexAttribPointer(
            ii, info.size(), info.type(), info.normalized(), 0,
            array_buffer_offset_);
        array_buffer_offset_ += RoundUpToMultipleOf4(bytes_collected);
        GPU_DCHECK_LE(array_buffer_offset_, array_buffer_size_);
      }
    }
  }

  // Copies in indices to the service and returns the highest index accessed + 1
  GLsizei SetupSimulatedIndexBuffer(
      GLES2Implementation* gl,
      GLES2CmdHelper* gl_helper,
      GLsizei count,
      GLenum type,
      const void* indices) {
    gl_helper->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_array_buffer_id_);
    GLsizei bytes_per_element =
        GLES2Util::GetGLTypeSizeForTexturesAndBuffers(type);
    GLsizei bytes_needed = bytes_per_element * count;
    if (bytes_needed > element_array_buffer_size_) {
      element_array_buffer_size_ = bytes_needed;
      gl->BufferDataHelper(
          GL_ELEMENT_ARRAY_BUFFER, bytes_needed, NULL, GL_DYNAMIC_DRAW);
    }
    gl->BufferSubDataHelper(
        GL_ELEMENT_ARRAY_BUFFER, 0, bytes_needed, indices);
    GLsizei max_index = -1;
    switch (type) {
      case GL_UNSIGNED_BYTE: {
          const uint8* src = static_cast<const uint8*>(indices);
          for (GLsizei ii = 0; ii < count; ++ii) {
            if (src[ii] > max_index) {
              max_index = src[ii];
            }
          }
          break;
        }
      case GL_UNSIGNED_SHORT: {
          const uint16* src = static_cast<const uint16*>(indices);
          for (GLsizei ii = 0; ii < count; ++ii) {
            if (src[ii] > max_index) {
              max_index = src[ii];
            }
          }
          break;
        }
      default:
        break;
    }
    return max_index + 1;
  }

 private:
  GLuint max_vertex_attribs_;
  GLuint num_client_side_pointers_enabled_;
  GLuint array_buffer_id_;
  GLsizei array_buffer_size_;
  GLsizei array_buffer_offset_;
  GLuint element_array_buffer_id_;
  GLsizei element_array_buffer_size_;
  scoped_array<VertexAttribInfo> vertex_attrib_infos_;
  GLsizei collection_buffer_size_;
  scoped_array<int8> collection_buffer_;

  DISALLOW_COPY_AND_ASSIGN(ClientSideBufferHelper);
};

#if !defined(_MSC_VER)
const size_t GLES2Implementation::kMaxSizeOfSimpleResult;
const unsigned int GLES2Implementation::kStartingOffset;
#endif

GLES2Implementation::SingleThreadChecker::SingleThreadChecker(
    GLES2Implementation* gles2_implementation)
    : gles2_implementation_(gles2_implementation) {
  GPU_CHECK_EQ(0, gles2_implementation_->use_count_);
  ++gles2_implementation_->use_count_;
}

GLES2Implementation::SingleThreadChecker::~SingleThreadChecker() {
  --gles2_implementation_->use_count_;
  GPU_CHECK_EQ(0, gles2_implementation_->use_count_);
}

GLES2Implementation::GLES2Implementation(
      GLES2CmdHelper* helper,
      ShareGroup* share_group,
      TransferBufferInterface* transfer_buffer,
      bool share_resources,
      bool bind_generates_resource)
    : helper_(helper),
      transfer_buffer_(transfer_buffer),
      angle_pack_reverse_row_order_status(kUnknownExtensionStatus),
      pack_alignment_(4),
      unpack_alignment_(4),
      unpack_flip_y_(false),
      unpack_row_length_(0),
      unpack_skip_rows_(0),
      unpack_skip_pixels_(0),
      pack_reverse_row_order_(false),
      active_texture_unit_(0),
      bound_framebuffer_(0),
      bound_renderbuffer_(0),
      bound_array_buffer_id_(0),
      bound_element_array_buffer_id_(0),
      client_side_array_id_(0),
      client_side_element_array_id_(0),
      error_bits_(0),
      debug_(false),
      use_count_(0),
      current_query_(NULL),
      error_message_callback_(NULL) {
  GPU_DCHECK(helper);
  GPU_DCHECK(transfer_buffer);
  GPU_CLIENT_LOG_CODE_BLOCK({
    debug_ = CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableGPUClientLogging);
  });

  share_group_ = (share_group ? share_group : new ShareGroup(
      share_resources, bind_generates_resource));

  memset(&reserved_ids_, 0, sizeof(reserved_ids_));
}

bool GLES2Implementation::Initialize(
    unsigned int starting_transfer_buffer_size,
    unsigned int min_transfer_buffer_size,
    unsigned int max_transfer_buffer_size) {
  GPU_DCHECK_GE(starting_transfer_buffer_size, min_transfer_buffer_size);
  GPU_DCHECK_LE(starting_transfer_buffer_size, max_transfer_buffer_size);
  GPU_DCHECK_GE(min_transfer_buffer_size, kStartingOffset);

  if (!transfer_buffer_->Initialize(
      starting_transfer_buffer_size,
      kStartingOffset,
      min_transfer_buffer_size,
      max_transfer_buffer_size,
      kAlignment,
      kSizeToFlush)) {
    return false;
  }

  mapped_memory_.reset(new MappedMemoryManager(helper_));
  SetSharedMemoryChunkSizeMultiple(1024 * 1024 * 2);

  static const GLenum pnames[] = {
    GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
    GL_MAX_CUBE_MAP_TEXTURE_SIZE,
    GL_MAX_FRAGMENT_UNIFORM_VECTORS,
    GL_MAX_RENDERBUFFER_SIZE,
    GL_MAX_TEXTURE_IMAGE_UNITS,
    GL_MAX_TEXTURE_SIZE,
    GL_MAX_VARYING_VECTORS,
    GL_MAX_VERTEX_ATTRIBS,
    GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,
    GL_MAX_VERTEX_UNIFORM_VECTORS,
    GL_NUM_COMPRESSED_TEXTURE_FORMATS,
    GL_NUM_SHADER_BINARY_FORMATS,
  };

  GetMultipleIntegervCHROMIUM(
      pnames, arraysize(pnames),
      &gl_state_.int_state.max_combined_texture_image_units,
      sizeof(gl_state_.int_state));

  util_.set_num_compressed_texture_formats(
      gl_state_.int_state.num_compressed_texture_formats);
  util_.set_num_shader_binary_formats(
      gl_state_.int_state.num_shader_binary_formats);

  texture_units_.reset(
      new TextureUnit[gl_state_.int_state.max_combined_texture_image_units]);

  query_tracker_.reset(new QueryTracker(mapped_memory_.get()));

#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  GetIdHandler(id_namespaces::kBuffers)->MakeIds(
      this, kClientSideArrayId, arraysize(reserved_ids_), &reserved_ids_[0]);

  client_side_buffer_helper_.reset(new ClientSideBufferHelper(
      gl_state_.int_state.max_vertex_attribs,
      reserved_ids_[0],
      reserved_ids_[1]));
#endif

  return true;
}

GLES2Implementation::~GLES2Implementation() {
  // Make sure the queries are finished otherwise we'll delete the
  // shared memory (mapped_memory_) which will free the memory used
  // by the queries. The GPU process when validating that memory is still
  // shared will fail and abort (ie, it will stop running).
  Finish();
  query_tracker_.reset();

#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  DeleteBuffers(arraysize(reserved_ids_), &reserved_ids_[0]);
#endif
  // The share group needs to be able to use a command buffer to talk
  // to service if it's destroyed so set one for it then release the reference.
  // If it's destroyed it will use this GLES2Implemenation.
  share_group_->SetGLES2ImplementationForDestruction(this);
  share_group_ = NULL;
  // Make sure the commands make it the service.
  Finish();
}

GLuint GLES2Implementation::MakeTextureId() {
  GLuint id;
  GetIdHandler(id_namespaces::kTextures)->MakeIds(this, 0, 1, &id);
  return id;
}

void GLES2Implementation::FreeTextureId(GLuint id) {
  GetIdHandler(id_namespaces::kTextures)->FreeIds(
      this, 1, &id, &GLES2Implementation::DeleteTexturesStub);
}

IdHandlerInterface* GLES2Implementation::GetIdHandler(int namespace_id) const {
  return share_group_->GetIdHandler(namespace_id);
}

void* GLES2Implementation::GetResultBuffer() {
  return transfer_buffer_->GetResultBuffer();
}

int32 GLES2Implementation::GetResultShmId() {
  return transfer_buffer_->GetShmId();
}

uint32 GLES2Implementation::GetResultShmOffset() {
  return transfer_buffer_->GetResultOffset();
}

void GLES2Implementation::SetSharedMemoryChunkSizeMultiple(
    unsigned int multiple) {
  mapped_memory_->set_chunk_size_multiple(multiple);
}

void GLES2Implementation::FreeUnusedSharedMemory() {
  mapped_memory_->FreeUnused();
}

void GLES2Implementation::FreeEverything() {
  Finish();
  FreeUnusedSharedMemory();
  transfer_buffer_->Free();
  helper_->FreeRingBuffer();
}

void GLES2Implementation::WaitForCmd() {
  TRACE_EVENT0("gpu", "GLES2::WaitForCmd");
  helper_->CommandBufferHelper::Finish();
}

bool GLES2Implementation::IsExtensionAvailable(const char* ext) {
  const char* extensions =
      reinterpret_cast<const char*>(GetStringHelper(GL_EXTENSIONS));
  int length = strlen(ext);
  while (true) {
    int n = strcspn(extensions, " ");
    if (n == length && 0 == strncmp(ext, extensions, length)) {
      return true;
    }
    if ('\0' == extensions[n]) {
      return false;
    }
    extensions += n + 1;
  }
}

bool GLES2Implementation::IsAnglePackReverseRowOrderAvailable() {
  switch (angle_pack_reverse_row_order_status) {
    case kAvailableExtensionStatus:
      return true;
    case kUnavailableExtensionStatus:
      return false;
    default:
      if (IsExtensionAvailable("GL_ANGLE_pack_reverse_row_order")) {
          angle_pack_reverse_row_order_status = kAvailableExtensionStatus;
          return true;
      } else {
          angle_pack_reverse_row_order_status = kUnavailableExtensionStatus;
          return false;
      }
  }
}

GLenum GLES2Implementation::GetError() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGetError()");
  GLenum err = GetGLError();
  GPU_CLIENT_LOG("returned " << GLES2Util::GetStringError(err));
  return err;
}

GLenum GLES2Implementation::GetGLError() {
  TRACE_EVENT0("gpu", "GLES2::GetGLError");
  // Check the GL error first, then our wrapped error.
  typedef gles2::GetError::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return GL_NO_ERROR;
  }
  *result = GL_NO_ERROR;
  helper_->GetError(GetResultShmId(), GetResultShmOffset());
  WaitForCmd();
  GLenum error = *result;
  if (error == GL_NO_ERROR && error_bits_ != 0) {
    for (uint32 mask = 1; mask != 0; mask = mask << 1) {
      if ((error_bits_ & mask) != 0) {
        error = GLES2Util::GLErrorBitToGLError(mask);
        break;
      }
    }
  }

  if (error != GL_NO_ERROR) {
    // There was an error, clear the corresponding wrapped error.
    error_bits_ &= ~GLES2Util::GLErrorToErrorBit(error);
  }
  return error;
}

void GLES2Implementation::SetGLError(GLenum error, const char* msg) {
  GPU_CLIENT_LOG("[" << this << "] Client Synthesized Error: "
                 << GLES2Util::GetStringError(error) << ": " << msg);
  if (msg) {
    last_error_ = msg;
  }
  if (error_message_callback_) {
    std::string temp(GLES2Util::GetStringError(error)  + " : " +
                     (msg ? msg : ""));
    error_message_callback_->OnErrorMessage(temp.c_str(), 0);
  }
  error_bits_ |= GLES2Util::GLErrorToErrorBit(error);
}

bool GLES2Implementation::GetBucketContents(uint32 bucket_id,
                                            std::vector<int8>* data) {
  TRACE_EVENT0("gpu", "GLES2::GetBucketContents");
  GPU_DCHECK(data);
  const uint32 kStartSize = 32 * 1024;
  ScopedTransferBufferPtr buffer(kStartSize, helper_, transfer_buffer_);
  if (!buffer.valid()) {
    return false;
  }
  typedef cmd::GetBucketStart::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return false;
  }
  *result = 0;
  helper_->GetBucketStart(
      bucket_id, GetResultShmId(), GetResultShmOffset(),
      buffer.size(), buffer.shm_id(), buffer.offset());
  WaitForCmd();
  uint32 size = *result;
  data->resize(size);
  if (size > 0u) {
    uint32 offset = 0;
    while (size) {
      if (!buffer.valid()) {
        buffer.Reset(size);
        if (!buffer.valid()) {
          return false;
        }
        helper_->GetBucketData(
            bucket_id, offset, buffer.size(), buffer.shm_id(), buffer.offset());
        WaitForCmd();
      }
      uint32 size_to_copy = std::min(size, buffer.size());
      memcpy(&(*data)[offset], buffer.address(), size_to_copy);
      offset += size_to_copy;
      size -= size_to_copy;
      buffer.Release();
    };
    // Free the bucket. This is not required but it does free up the memory.
    // and we don't have to wait for the result so from the client's perspective
    // it's cheap.
    helper_->SetBucketSize(bucket_id, 0);
  }
  return true;
}

void GLES2Implementation::SetBucketContents(
    uint32 bucket_id, const void* data, size_t size) {
  GPU_DCHECK(data);
  helper_->SetBucketSize(bucket_id, size);
  if (size > 0u) {
    uint32 offset = 0;
    while (size) {
      ScopedTransferBufferPtr buffer(size, helper_, transfer_buffer_);
      if (!buffer.valid()) {
        return;
      }
      memcpy(buffer.address(), static_cast<const int8*>(data) + offset,
             buffer.size());
      helper_->SetBucketData(
          bucket_id, offset, buffer.size(), buffer.shm_id(), buffer.offset());
      offset += buffer.size();
      size -= buffer.size();
    }
  }
}

void GLES2Implementation::SetBucketAsCString(
    uint32 bucket_id, const char* str) {
  // NOTE: strings are passed NULL terminated. That means the empty
  // string will have a size of 1 and no-string will have a size of 0
  if (str) {
    SetBucketContents(bucket_id, str, strlen(str) + 1);
  } else {
    helper_->SetBucketSize(bucket_id, 0);
  }
}

bool GLES2Implementation::GetBucketAsString(
    uint32 bucket_id, std::string* str) {
  GPU_DCHECK(str);
  std::vector<int8> data;
  // NOTE: strings are passed NULL terminated. That means the empty
  // string will have a size of 1 and no-string will have a size of 0
  if (!GetBucketContents(bucket_id, &data)) {
    return false;
  }
  if (data.empty()) {
    return false;
  }
  str->assign(&data[0], &data[0] + data.size() - 1);
  return true;
}

void GLES2Implementation::SetBucketAsString(
    uint32 bucket_id, const std::string& str) {
  // NOTE: strings are passed NULL terminated. That means the empty
  // string will have a size of 1 and no-string will have a size of 0
  SetBucketContents(bucket_id, str.c_str(), str.size() + 1);
}

bool GLES2Implementation::SetCapabilityState(GLenum cap, bool enabled) {
  switch (cap) {
    case GL_DITHER:
      gl_state_.enable_state.dither = enabled;
      return true;
    case GL_BLEND:
      gl_state_.enable_state.blend = enabled;
      return true;
    case GL_CULL_FACE:
      gl_state_.enable_state.cull_face = enabled;
      return true;
    case GL_DEPTH_TEST:
      gl_state_.enable_state.depth_test = enabled;
      return true;
    case GL_POLYGON_OFFSET_FILL:
      gl_state_.enable_state.polygon_offset_fill = enabled;
      return true;
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
      gl_state_.enable_state.sample_alpha_to_coverage = enabled;
      return true;
    case GL_SAMPLE_COVERAGE:
      gl_state_.enable_state.sample_coverage = enabled;
      return true;
    case GL_SCISSOR_TEST:
      gl_state_.enable_state.scissor_test = enabled;
      return true;
    case GL_STENCIL_TEST:
      gl_state_.enable_state.stencil_test = enabled;
      return true;
    default:
      return false;
  }
}

void GLES2Implementation::Disable(GLenum cap) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glDisable("
                 << GLES2Util::GetStringCapability(cap) << ")");
  SetCapabilityState(cap, false);
  helper_->Disable(cap);
}

void GLES2Implementation::Enable(GLenum cap) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glEnable("
                 << GLES2Util::GetStringCapability(cap) << ")");
  SetCapabilityState(cap, true);
  helper_->Enable(cap);
}

GLboolean GLES2Implementation::IsEnabled(GLenum cap) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glIsEnabled("
                 << GLES2Util::GetStringCapability(cap) << ")");
  bool state = false;
  switch (cap) {
    case GL_DITHER:
      state = gl_state_.enable_state.dither;
      break;
    case GL_BLEND:
      state = gl_state_.enable_state.blend;
      break;
    case GL_CULL_FACE:
      state = gl_state_.enable_state.cull_face;
      break;
    case GL_DEPTH_TEST:
      state = gl_state_.enable_state.depth_test;
      break;
    case GL_POLYGON_OFFSET_FILL:
      state = gl_state_.enable_state.polygon_offset_fill;
      break;
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
      state = gl_state_.enable_state.sample_alpha_to_coverage;
      break;
    case GL_SAMPLE_COVERAGE:
      state = gl_state_.enable_state.sample_coverage;
      break;
    case GL_SCISSOR_TEST:
      state = gl_state_.enable_state.scissor_test;
      break;
    case GL_STENCIL_TEST:
      state = gl_state_.enable_state.stencil_test;
      break;
    default: {
      typedef IsEnabled::Result Result;
      Result* result = GetResultAs<Result*>();
      if (!result) {
        return GL_FALSE;
      }
      *result = 0;
      helper_->IsEnabled(cap, GetResultShmId(), GetResultShmOffset());
      WaitForCmd();
      state = (*result) != 0;
      break;
    }
  }
  GPU_CLIENT_LOG("returned " << state);
  return state;
}

bool GLES2Implementation::GetHelper(GLenum pname, GLint* params) {
  switch (pname) {
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
      *params = gl_state_.int_state.max_combined_texture_image_units;
      return true;
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      *params = gl_state_.int_state.max_cube_map_texture_size;
      return true;
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      *params = gl_state_.int_state.max_fragment_uniform_vectors;
      return true;
    case GL_MAX_RENDERBUFFER_SIZE:
      *params = gl_state_.int_state.max_renderbuffer_size;
      return true;
    case GL_MAX_TEXTURE_IMAGE_UNITS:
      *params = gl_state_.int_state.max_texture_image_units;
      return true;
    case GL_MAX_TEXTURE_SIZE:
      *params = gl_state_.int_state.max_texture_size;
      return true;
    case GL_MAX_VARYING_VECTORS:
      *params = gl_state_.int_state.max_varying_vectors;
      return true;
    case GL_MAX_VERTEX_ATTRIBS:
      *params = gl_state_.int_state.max_vertex_attribs;
      return true;
    case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
      *params = gl_state_.int_state.max_vertex_texture_image_units;
      return true;
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
      *params = gl_state_.int_state.max_vertex_uniform_vectors;
      return true;
    case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
      *params = gl_state_.int_state.num_compressed_texture_formats;
      return true;
    case GL_NUM_SHADER_BINARY_FORMATS:
      *params = gl_state_.int_state.num_shader_binary_formats;
      return true;
    case GL_ARRAY_BUFFER_BINDING:
      if (share_group_->bind_generates_resource()) {
        *params = bound_array_buffer_id_;
        return true;
      }
      return false;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
      if (share_group_->bind_generates_resource()) {
        *params = bound_element_array_buffer_id_;
        return true;
      }
      return false;
    case GL_ACTIVE_TEXTURE:
      *params = active_texture_unit_ + GL_TEXTURE0;
      return true;
    case GL_TEXTURE_BINDING_2D:
      if (share_group_->bind_generates_resource()) {
        *params = texture_units_[active_texture_unit_].bound_texture_2d;
        return true;
      }
      return false;
    case GL_TEXTURE_BINDING_CUBE_MAP:
      if (share_group_->bind_generates_resource()) {
        *params = texture_units_[active_texture_unit_].bound_texture_cube_map;
        return true;
      }
      return false;
    case GL_FRAMEBUFFER_BINDING:
      if (share_group_->bind_generates_resource()) {
        *params = bound_framebuffer_;
        return true;
      }
      return false;
    case GL_RENDERBUFFER_BINDING:
      if (share_group_->bind_generates_resource()) {
        *params = bound_renderbuffer_;
        return true;
      }
      return false;
    default:
      return false;
  }
}

bool GLES2Implementation::GetBooleanvHelper(GLenum pname, GLboolean* params) {
  // TODO(gman): Make this handle pnames that return more than 1 value.
  GLint value;
  if (!GetHelper(pname, &value)) {
    return false;
  }
  *params = static_cast<GLboolean>(value);
  return true;
}

bool GLES2Implementation::GetFloatvHelper(GLenum pname, GLfloat* params) {
  // TODO(gman): Make this handle pnames that return more than 1 value.
  GLint value;
  if (!GetHelper(pname, &value)) {
    return false;
  }
  *params = static_cast<GLfloat>(value);
  return true;
}

bool GLES2Implementation::GetIntegervHelper(GLenum pname, GLint* params) {
  return GetHelper(pname, params);
}

GLuint GLES2Implementation::GetMaxValueInBufferCHROMIUMHelper(
    GLuint buffer_id, GLsizei count, GLenum type, GLuint offset) {
  typedef GetMaxValueInBufferCHROMIUM::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return 0;
  }
  *result = 0;
  helper_->GetMaxValueInBufferCHROMIUM(
      buffer_id, count, type, offset, GetResultShmId(), GetResultShmOffset());
  WaitForCmd();
  return *result;
}

GLuint GLES2Implementation::GetMaxValueInBufferCHROMIUM(
    GLuint buffer_id, GLsizei count, GLenum type, GLuint offset) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGetMaxValueInBufferCHROMIUM("
                 << buffer_id << ", " << count << ", "
                 << GLES2Util::GetStringGetMaxIndexType(type)
                 << ", " << offset << ")");
  GLuint result = GetMaxValueInBufferCHROMIUMHelper(
      buffer_id, count, type, offset);
  GPU_CLIENT_LOG("returned " << result);
  return result;
}

void GLES2Implementation::DrawElements(
    GLenum mode, GLsizei count, GLenum type, const void* indices) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glDrawElements("
      << GLES2Util::GetStringDrawMode(mode) << ", "
      << count << ", "
      << GLES2Util::GetStringIndexType(type) << ", "
      << static_cast<const void*>(indices) << ")");
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glDrawElements: count less than 0.");
    return;
  }
  if (count == 0) {
    return;
  }
#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  bool have_client_side =
      client_side_buffer_helper_->HaveEnabledClientSideBuffers();
  GLsizei num_elements = 0;
  GLuint offset = ToGLuint(indices);
  if (bound_element_array_buffer_id_ == 0) {
    // Index buffer is client side array.
    // Copy to buffer, scan for highest index.
    num_elements = client_side_buffer_helper_->SetupSimulatedIndexBuffer(
        this, helper_, count, type, indices);
    offset = 0;
  } else {
    // Index buffer is GL buffer. Ask the service for the highest vertex
    // that will be accessed. Note: It doesn't matter if another context
    // changes the contents of any of the buffers. The service will still
    // validate the indices. We just need to know how much to copy across.
    if (have_client_side) {
      num_elements = GetMaxValueInBufferCHROMIUMHelper(
          bound_element_array_buffer_id_, count, type, ToGLuint(indices)) + 1;
    }
  }
  if (have_client_side) {
    client_side_buffer_helper_->SetupSimulatedClientSideBuffers(
        this, helper_, num_elements, 0);
  }
  helper_->DrawElements(mode, count, type, offset);
  if (have_client_side) {
    // Restore the user's current binding.
    helper_->BindBuffer(GL_ARRAY_BUFFER, bound_array_buffer_id_);
  }
  if (bound_element_array_buffer_id_ == 0) {
    // Restore the element array binding.
    helper_->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  }
#else
  helper_->DrawElements(mode, count, type, ToGLuint(indices));
#endif
}

void GLES2Implementation::Flush() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glFlush()");
  // Insert the cmd to call glFlush
  helper_->Flush();
  // Flush our command buffer
  // (tell the service to execute up to the flush cmd.)
  helper_->CommandBufferHelper::Flush();
}

void GLES2Implementation::Finish() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glFinish()");
  TRACE_EVENT0("gpu", "GLES2::Finish");
  // Insert the cmd to call glFinish
  helper_->Finish();
  // Finish our command buffer
  // (tell the service to execute up to the Finish cmd and wait for it to
  // execute.)
  helper_->CommandBufferHelper::Finish();
}

void GLES2Implementation::SwapBuffers() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glSwapBuffers()");
  // TODO(piman): Strictly speaking we'd want to insert the token after the
  // swap, but the state update with the updated token might not have happened
  // by the time the SwapBuffer callback gets called, forcing us to synchronize
  // with the GPU process more than needed. So instead, make it happen before.
  // All it means is that we could be slightly looser on the kMaxSwapBuffers
  // semantics if the client doesn't use the callback mechanism, and by chance
  // the scheduler yields between the InsertToken and the SwapBuffers.
  swap_buffers_tokens_.push(helper_->InsertToken());
  helper_->SwapBuffers();
  helper_->CommandBufferHelper::Flush();
  // Wait if we added too many swap buffers. Add 1 to kMaxSwapBuffers to
  // compensate for TODO above.
  if (swap_buffers_tokens_.size() > kMaxSwapBuffers + 1) {
    helper_->WaitForToken(swap_buffers_tokens_.front());
    swap_buffers_tokens_.pop();
  }
}

void GLES2Implementation::GenSharedIdsCHROMIUM(
  GLuint namespace_id, GLuint id_offset, GLsizei n, GLuint* ids) {
  GPU_CLIENT_LOG("[" << this << "] glGenSharedIdsCHROMIUM("
      << namespace_id << ", " << id_offset << ", " << n << ", " <<
      static_cast<void*>(ids) << ")");
  TRACE_EVENT0("gpu", "GLES2::GenSharedIdsCHROMIUM");
  GLsizei num = n;
  GLuint* dst = ids;
  while (num) {
    ScopedTransferBufferArray<GLint> id_buffer(num, helper_, transfer_buffer_);
    if (!id_buffer.valid()) {
      return;
    }
    helper_->GenSharedIdsCHROMIUM(
        namespace_id, id_offset, id_buffer.num_elements(),
        id_buffer.shm_id(), id_buffer.offset());
    WaitForCmd();
    memcpy(dst, id_buffer.address(), sizeof(*dst) * id_buffer.num_elements());
    num -= id_buffer.num_elements();
    dst += id_buffer.num_elements();
  }
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << namespace_id << ", " << ids[i]);
    }
  });
}

void GLES2Implementation::DeleteSharedIdsCHROMIUM(
    GLuint namespace_id, GLsizei n, const GLuint* ids) {
  GPU_CLIENT_LOG("[" << this << "] glDeleteSharedIdsCHROMIUM("
      << namespace_id << ", " << n << ", "
      << static_cast<const void*>(ids) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << namespace_id << ", "  << ids[i]);
    }
  });
  TRACE_EVENT0("gpu", "GLES2::DeleteSharedIdsCHROMIUM");
  while (n) {
    ScopedTransferBufferArray<GLint> id_buffer(n, helper_, transfer_buffer_);
    if (!id_buffer.valid()) {
      return;
    }
    memcpy(id_buffer.address(), ids, sizeof(*ids) * id_buffer.num_elements());
    helper_->DeleteSharedIdsCHROMIUM(
        namespace_id, id_buffer.num_elements(),
        id_buffer.shm_id(), id_buffer.offset());
    WaitForCmd();
    n -= id_buffer.num_elements();
    ids += id_buffer.num_elements();
  }
}

void GLES2Implementation::RegisterSharedIdsCHROMIUM(
    GLuint namespace_id, GLsizei n, const GLuint* ids) {
  GPU_CLIENT_LOG("[" << this << "] glRegisterSharedIdsCHROMIUM("
     << namespace_id << ", " << n << ", "
     << static_cast<const void*>(ids) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": "  << namespace_id << ", " << ids[i]);
    }
  });
  TRACE_EVENT0("gpu", "GLES2::RegisterSharedIdsCHROMIUM");
  while (n) {
    ScopedTransferBufferArray<GLint> id_buffer(n, helper_, transfer_buffer_);
    if (!id_buffer.valid()) {
      return;
    }
    memcpy(id_buffer.address(), ids, sizeof(*ids) * id_buffer.num_elements());
    helper_->RegisterSharedIdsCHROMIUM(
        namespace_id, id_buffer.num_elements(),
        id_buffer.shm_id(), id_buffer.offset());
    WaitForCmd();
    n -= id_buffer.num_elements();
    ids += id_buffer.num_elements();
  }
}

void GLES2Implementation::BindAttribLocation(
  GLuint program, GLuint index, const char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glBindAttribLocation(" << program << ", "
      << index << ", " << name << ")");
  SetBucketAsString(kResultBucketId, name);
  helper_->BindAttribLocationBucket(program, index, kResultBucketId);
  helper_->SetBucketSize(kResultBucketId, 0);
}

void GLES2Implementation::GetVertexAttribPointerv(
    GLuint index, GLenum pname, void** ptr) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGetVertexAttribPointer(" << index << ", "
      << GLES2Util::GetStringVertexPointer(pname) << ", "
      << static_cast<void*>(ptr) << ")");

#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  // If it's a client side buffer the client has the data.
  if (client_side_buffer_helper_->GetAttribPointer(index, pname, ptr)) {
    return;
  }
#endif  // defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)

  TRACE_EVENT0("gpu", "GLES2::GetVertexAttribPointerv");
  typedef gles2::GetVertexAttribPointerv::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetVertexAttribPointerv(
    index, pname, GetResultShmId(), GetResultShmOffset());
  WaitForCmd();
  result->CopyResult(ptr);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
}

bool GLES2Implementation::DeleteProgramHelper(GLuint program) {
  if (!GetIdHandler(id_namespaces::kProgramsAndShaders)->FreeIds(
      this, 1, &program, &GLES2Implementation::DeleteProgramStub)) {
    SetGLError(
        GL_INVALID_VALUE,
        "glDeleteProgram: id not created by this context.");
    return false;
  }
  return true;
}

void GLES2Implementation::DeleteProgramStub(
    GLsizei n, const GLuint* programs) {
  GPU_DCHECK_EQ(1, n);
  share_group_->program_info_manager()->DeleteInfo(programs[0]);
  helper_->DeleteProgram(programs[0]);
}

bool GLES2Implementation::DeleteShaderHelper(GLuint shader) {
  if (!GetIdHandler(id_namespaces::kProgramsAndShaders)->FreeIds(
      this, 1, &shader, &GLES2Implementation::DeleteShaderStub)) {
    SetGLError(
        GL_INVALID_VALUE,
        "glDeleteShader: id not created by this context.");
    return false;
  }
  return true;
}

void GLES2Implementation::DeleteShaderStub(
    GLsizei n, const GLuint* shaders) {
  GPU_DCHECK_EQ(1, n);
  share_group_->program_info_manager()->DeleteInfo(shaders[0]);
  helper_->DeleteShader(shaders[0]);
}


GLint GLES2Implementation::GetAttribLocationHelper(
    GLuint program, const char* name) {
  typedef GetAttribLocationBucket::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return -1;
  }
  *result = -1;
  SetBucketAsCString(kResultBucketId, name);
  helper_->GetAttribLocationBucket(
      program, kResultBucketId, GetResultShmId(), GetResultShmOffset());
  WaitForCmd();
  helper_->SetBucketSize(kResultBucketId, 0);
  return *result;
}

GLint GLES2Implementation::GetAttribLocation(
    GLuint program, const char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGetAttribLocation(" << program
      << ", " << name << ")");
  TRACE_EVENT0("gpu", "GLES2::GetAttribLocation");
  GLint loc = share_group_->program_info_manager()->GetAttribLocation(
      this, program, name);
  GPU_CLIENT_LOG("returned " << loc);
  return loc;
}

GLint GLES2Implementation::GetUniformLocationHelper(
    GLuint program, const char* name) {
  typedef GetUniformLocationBucket::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return -1;
  }
  *result = -1;
  SetBucketAsCString(kResultBucketId, name);
  helper_->GetUniformLocationBucket(program, kResultBucketId,
                                    GetResultShmId(), GetResultShmOffset());
  WaitForCmd();
  helper_->SetBucketSize(kResultBucketId, 0);
  return *result;
}

GLint GLES2Implementation::GetUniformLocation(
    GLuint program, const char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGetUniformLocation(" << program
      << ", " << name << ")");
  TRACE_EVENT0("gpu", "GLES2::GetUniformLocation");
  GLint loc = share_group_->program_info_manager()->GetUniformLocation(
      this, program, name);
  GPU_CLIENT_LOG("returned " << loc);
  return loc;
}

bool GLES2Implementation::GetProgramivHelper(
    GLuint program, GLenum pname, GLint* params) {
  bool got_value = share_group_->program_info_manager()->GetProgramiv(
      this, program, pname, params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    if (got_value) {
      GPU_CLIENT_LOG("  0: " << *params);
    }
  });
  return got_value;
}

void GLES2Implementation::LinkProgram(GLuint program) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glLinkProgram(" << program << ")");
  helper_->LinkProgram(program);
  share_group_->program_info_manager()->CreateInfo(program);
}

void GLES2Implementation::ShaderBinary(
    GLsizei n, const GLuint* shaders, GLenum binaryformat, const void* binary,
    GLsizei length) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glShaderBinary(" << n << ", "
      << static_cast<const void*>(shaders) << ", "
      << GLES2Util::GetStringEnum(binaryformat) << ", "
      << static_cast<const void*>(binary) << ", "
      << length << ")");
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glShaderBinary n < 0.");
    return;
  }
  if (length < 0) {
    SetGLError(GL_INVALID_VALUE, "glShaderBinary length < 0.");
    return;
  }
  // TODO(gman): ShaderBinary should use buckets.
  unsigned int shader_id_size = n * sizeof(*shaders);
  ScopedTransferBufferArray<GLint> buffer(
      shader_id_size + length, helper_, transfer_buffer_);
  if (!buffer.valid() || buffer.num_elements() != shader_id_size + length) {
    SetGLError(GL_OUT_OF_MEMORY, "glShaderBinary: out of memory.");
    return;
  }
  void* shader_ids = buffer.elements();
  void* shader_data = buffer.elements() + shader_id_size;
  memcpy(shader_ids, shaders, shader_id_size);
  memcpy(shader_data, binary, length);
  helper_->ShaderBinary(
      n,
      buffer.shm_id(),
      buffer.offset(),
      binaryformat,
      buffer.shm_id(),
      buffer.offset() + shader_id_size,
      length);
}

void GLES2Implementation::PixelStorei(GLenum pname, GLint param) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glPixelStorei("
      << GLES2Util::GetStringPixelStore(pname) << ", "
      << param << ")");
  switch (pname) {
    case GL_PACK_ALIGNMENT:
        pack_alignment_ = param;
        break;
    case GL_UNPACK_ALIGNMENT:
        unpack_alignment_ = param;
        break;
    case GL_UNPACK_ROW_LENGTH:
        unpack_row_length_ = param;
        return;
    case GL_UNPACK_SKIP_ROWS:
        unpack_skip_rows_ = param;
        return;
    case GL_UNPACK_SKIP_PIXELS:
        unpack_skip_pixels_ = param;
        return;
    case GL_UNPACK_FLIP_Y_CHROMIUM:
        unpack_flip_y_ = (param != 0);
        break;
    case GL_PACK_REVERSE_ROW_ORDER_ANGLE:
        pack_reverse_row_order_ =
            IsAnglePackReverseRowOrderAvailable() ? (param != 0) : false;
        break;
    default:
        break;
  }
  helper_->PixelStorei(pname, param);
}


void GLES2Implementation::VertexAttribPointer(
    GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
    const void* ptr) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glVertexAttribPointer("
      << index << ", "
      << size << ", "
      << GLES2Util::GetStringVertexAttribType(type) << ", "
      << GLES2Util::GetStringBool(normalized) << ", "
      << stride << ", "
      << static_cast<const void*>(ptr) << ")");
#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  // Record the info on the client side.
  client_side_buffer_helper_->SetAttribPointer(
      bound_array_buffer_id_, index, size, type, normalized, stride, ptr);
  if (bound_array_buffer_id_ != 0) {
    // Only report NON client side buffers to the service.
    helper_->VertexAttribPointer(index, size, type, normalized, stride,
                                 ToGLuint(ptr));
  }
#else  // !defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  helper_->VertexAttribPointer(index, size, type, normalized, stride,
                               ToGLuint(ptr));
#endif  // !defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
}

void GLES2Implementation::VertexAttribDivisorANGLE(
    GLuint index, GLuint divisor) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glVertexAttribDivisorANGLE("
      << index << ", "
      << divisor << ") ");
#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  // Record the info on the client side.
  client_side_buffer_helper_->SetAttribDivisor(index, divisor);
#endif // defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  helper_->VertexAttribDivisorANGLE(index, divisor);
}

void GLES2Implementation::ShaderSource(
    GLuint shader, GLsizei count, const char** source, const GLint* length) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glShaderSource("
      << shader << ", " << count << ", "
      << static_cast<const void*>(source) << ", "
      << static_cast<const void*>(length) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei ii = 0; ii < count; ++ii) {
      if (source[ii]) {
        if (length && length[ii] >= 0) {
          std::string str(source[ii], length[ii]);
          GPU_CLIENT_LOG("  " << ii << ": ---\n" << str << "\n---");
        } else {
          GPU_CLIENT_LOG("  " << ii << ": ---\n" << source[ii] << "\n---");
        }
      } else {
        GPU_CLIENT_LOG("  " << ii << ": NULL");
      }
    }
  });
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glShaderSource count < 0");
    return;
  }
  if (shader == 0) {
    SetGLError(GL_INVALID_VALUE, "glShaderSource shader == 0");
    return;
  }

  // Compute the total size.
  uint32 total_size = 1;
  for (GLsizei ii = 0; ii < count; ++ii) {
    if (source[ii]) {
      total_size += (length && length[ii] >= 0) ?
          static_cast<size_t>(length[ii]) : strlen(source[ii]);
    }
  }

  // Concatenate all the strings in to a bucket on the service.
  helper_->SetBucketSize(kResultBucketId, total_size);
  uint32 offset = 0;
  for (GLsizei ii = 0; ii <= count; ++ii) {
    const char* src = ii < count ? source[ii] : "";
    if (src) {
      uint32 size = ii < count ?
          (length ? static_cast<size_t>(length[ii]) : strlen(src)) : 1;
      while (size) {
        ScopedTransferBufferPtr buffer(size, helper_, transfer_buffer_);
        if (!buffer.valid()) {
          return;
        }
        memcpy(buffer.address(), src, buffer.size());
        helper_->SetBucketData(kResultBucketId, offset, buffer.size(),
                               buffer.shm_id(), buffer.offset());
        offset += buffer.size();
        src += buffer.size();
        size -= buffer.size();
      }
    }
  }

  GPU_DCHECK_EQ(total_size, offset);

  helper_->ShaderSourceBucket(shader, kResultBucketId);
  helper_->SetBucketSize(kResultBucketId, 0);
}

void GLES2Implementation::BufferDataHelper(
    GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
  if (size == 0) {
    return;
  }

  if (size < 0) {
    SetGLError(GL_INVALID_VALUE, "glBufferData: size < 0");
    return;
  }

  // If there is no data just send BufferData
  if (!data) {
    helper_->BufferData(target, size, 0, 0, usage);
    return;
  }

  // See if we can send all at once.
  ScopedTransferBufferPtr buffer(size, helper_, transfer_buffer_);
  if (!buffer.valid()) {
    return;
  }

  if (buffer.size() >= static_cast<unsigned int>(size)) {
    memcpy(buffer.address(), data, size);
    helper_->BufferData(
        target,
        size,
        buffer.shm_id(),
        buffer.offset(),
        usage);
    return;
  }

  // Make the buffer with BufferData then send via BufferSubData
  helper_->BufferData(target, size, 0, 0, usage);
  BufferSubDataHelperImpl(target, 0, size, data, &buffer);
}

void GLES2Implementation::BufferData(
    GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glBufferData("
      << GLES2Util::GetStringBufferTarget(target) << ", "
      << size << ", "
      << static_cast<const void*>(data) << ", "
      << GLES2Util::GetStringBufferUsage(usage) << ")");
  BufferDataHelper(target, size, data, usage);
}

void GLES2Implementation::BufferSubDataHelper(
    GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
  if (size == 0) {
    return;
  }

  if (size < 0) {
    SetGLError(GL_INVALID_VALUE, "glBufferSubData: size < 0");
    return;
  }

  ScopedTransferBufferPtr buffer(size, helper_, transfer_buffer_);
  BufferSubDataHelperImpl(target, offset, size, data, &buffer);
}

void GLES2Implementation::BufferSubDataHelperImpl(
    GLenum target, GLintptr offset, GLsizeiptr size, const void* data,
    ScopedTransferBufferPtr* buffer) {
  GPU_DCHECK(buffer);
  GPU_DCHECK_GT(size, 0);

  const int8* source = static_cast<const int8*>(data);
  while (size) {
    if (!buffer->valid() || buffer->size() == 0) {
      buffer->Reset(size);
      if (!buffer->valid()) {
        return;
      }
    }
    memcpy(buffer->address(), source, buffer->size());
    helper_->BufferSubData(
        target, offset, buffer->size(), buffer->shm_id(), buffer->offset());
    offset += buffer->size();
    source += buffer->size();
    size -= buffer->size();
    buffer->Release();
  }
}

void GLES2Implementation::BufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glBufferSubData("
      << GLES2Util::GetStringBufferTarget(target) << ", "
      << offset << ", " << size << ", "
      << static_cast<const void*>(data) << ")");
  BufferSubDataHelper(target, offset, size, data);
}

void GLES2Implementation::CompressedTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLsizei width,
    GLsizei height, GLint border, GLsizei image_size, const void* data) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glCompressedTexImage2D("
      << GLES2Util::GetStringTextureTarget(target) << ", "
      << level << ", "
      << GLES2Util::GetStringCompressedTextureFormat(internalformat) << ", "
      << width << ", " << height << ", " << border << ", "
      << image_size << ", "
      << static_cast<const void*>(data) << ")");
  if (width < 0 || height < 0 || level < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexImage2D dimension < 0");
    return;
  }
  if (height == 0 || width == 0) {
    return;
  }
  SetBucketContents(kResultBucketId, data, image_size);
  helper_->CompressedTexImage2DBucket(
      target, level, internalformat, width, height, border, kResultBucketId);
  // Free the bucket. This is not required but it does free up the memory.
  // and we don't have to wait for the result so from the client's perspective
  // it's cheap.
  helper_->SetBucketSize(kResultBucketId, 0);
}

void GLES2Implementation::CompressedTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLsizei image_size, const void* data) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glCompressedTexSubImage2D("
      << GLES2Util::GetStringTextureTarget(target) << ", "
      << level << ", "
      << xoffset << ", " << yoffset << ", "
      << width << ", " << height << ", "
      << GLES2Util::GetStringCompressedTextureFormat(format) << ", "
      << image_size << ", "
      << static_cast<const void*>(data) << ")");
  if (width < 0 || height < 0 || level < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D dimension < 0");
    return;
  }
  SetBucketContents(kResultBucketId, data, image_size);
  helper_->CompressedTexSubImage2DBucket(
      target, level, xoffset, yoffset, width, height, format, kResultBucketId);
  // Free the bucket. This is not required but it does free up the memory.
  // and we don't have to wait for the result so from the client's perspective
  // it's cheap.
  helper_->SetBucketSize(kResultBucketId, 0);
}

namespace {

void CopyRectToBuffer(
    const void* pixels,
    uint32 height,
    uint32 unpadded_row_size,
    uint32 pixels_padded_row_size,
    bool flip_y,
    void* buffer,
    uint32 buffer_padded_row_size) {
  const int8* source = static_cast<const int8*>(pixels);
  int8* dest = static_cast<int8*>(buffer);
  if (flip_y || pixels_padded_row_size != buffer_padded_row_size) {
    if (flip_y) {
      dest += buffer_padded_row_size * (height - 1);
    }
    // the last row is copied unpadded at the end
    for (; height > 1; --height) {
      memcpy(dest, source, buffer_padded_row_size);
      if (flip_y) {
        dest -= buffer_padded_row_size;
      } else {
        dest += buffer_padded_row_size;
      }
      source += pixels_padded_row_size;
    }
    memcpy(dest, source, unpadded_row_size);
  } else {
    uint32 size = (height - 1) * pixels_padded_row_size + unpadded_row_size;
    memcpy(dest, source, size);
  }
}

}  // anonymous namespace

void GLES2Implementation::TexImage2D(
    GLenum target, GLint level, GLint internalformat, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glTexImage2D("
      << GLES2Util::GetStringTextureTarget(target) << ", "
      << level << ", "
      << GLES2Util::GetStringTextureInternalFormat(internalformat) << ", "
      << width << ", " << height << ", " << border << ", "
      << GLES2Util::GetStringTextureFormat(format) << ", "
      << GLES2Util::GetStringPixelType(type) << ", "
      << static_cast<const void*>(pixels) << ")");
  if (level < 0 || height < 0 || width < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexImage2D dimension < 0");
    return;
  }
  uint32 size;
  uint32 unpadded_row_size;
  uint32 padded_row_size;
  if (!GLES2Util::ComputeImageDataSizes(
          width, height, format, type, unpack_alignment_, &size,
          &unpadded_row_size, &padded_row_size)) {
    SetGLError(GL_INVALID_VALUE, "glTexImage2D: image size too large");
    return;
  }

  // If there's no data just issue TexImage2D
  if (!pixels) {
    helper_->TexImage2D(
       target, level, internalformat, width, height, border, format, type,
       0, 0);
    return;
  }

  // compute the advance bytes per row for the src pixels
  uint32 src_padded_row_size;
  if (unpack_row_length_ > 0) {
    if (!GLES2Util::ComputeImagePaddedRowSize(
        unpack_row_length_, format, type, unpack_alignment_,
        &src_padded_row_size)) {
      SetGLError(GL_INVALID_VALUE, "glTexImage2D: unpack row length too large");
      return;
    }
  } else {
    src_padded_row_size = padded_row_size;
  }

  // advance pixels pointer past the skip rows and skip pixels
  pixels = reinterpret_cast<const int8*>(pixels) +
      unpack_skip_rows_ * src_padded_row_size;
  if (unpack_skip_pixels_) {
    uint32 group_size = GLES2Util::ComputeImageGroupSize(format, type);
    pixels = reinterpret_cast<const int8*>(pixels) +
        unpack_skip_pixels_ * group_size;
  }

  // Check if we can send it all at once.
  ScopedTransferBufferPtr buffer(size, helper_, transfer_buffer_);
  if (!buffer.valid()) {
    return;
  }

  if (buffer.size() >= size) {
    CopyRectToBuffer(
        pixels, height, unpadded_row_size, src_padded_row_size, unpack_flip_y_,
        buffer.address(), padded_row_size);
    helper_->TexImage2D(
        target, level, internalformat, width, height, border, format, type,
        buffer.shm_id(), buffer.offset());
    return;
  }

  // No, so send it using TexSubImage2D.
  helper_->TexImage2D(
     target, level, internalformat, width, height, border, format, type,
     0, 0);
  TexSubImage2DImpl(
      target, level, 0, 0, width, height, format, type, unpadded_row_size,
      pixels, src_padded_row_size, GL_TRUE, &buffer, padded_row_size);
}

void GLES2Implementation::TexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void* pixels) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glTexSubImage2D("
      << GLES2Util::GetStringTextureTarget(target) << ", "
      << level << ", "
      << xoffset << ", " << yoffset << ", "
      << width << ", " << height << ", "
      << GLES2Util::GetStringTextureFormat(format) << ", "
      << GLES2Util::GetStringPixelType(type) << ", "
      << static_cast<const void*>(pixels) << ")");

  if (level < 0 || height < 0 || width < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexSubImage2D dimension < 0");
    return;
  }
  if (height == 0 || width == 0) {
    return;
  }

  uint32 temp_size;
  uint32 unpadded_row_size;
  uint32 padded_row_size;
  if (!GLES2Util::ComputeImageDataSizes(
        width, height, format, type, unpack_alignment_, &temp_size,
        &unpadded_row_size, &padded_row_size)) {
    SetGLError(GL_INVALID_VALUE, "glTexSubImage2D: size to large");
    return;
  }

  // compute the advance bytes per row for the src pixels
  uint32 src_padded_row_size;
  if (unpack_row_length_ > 0) {
    if (!GLES2Util::ComputeImagePaddedRowSize(
        unpack_row_length_, format, type, unpack_alignment_,
        &src_padded_row_size)) {
      SetGLError(GL_INVALID_VALUE, "glTexImage2D: unpack row length too large");
      return;
    }
  } else {
    src_padded_row_size = padded_row_size;
  }

  // advance pixels pointer past the skip rows and skip pixels
  pixels = reinterpret_cast<const int8*>(pixels) +
      unpack_skip_rows_ * src_padded_row_size;
  if (unpack_skip_pixels_) {
    uint32 group_size = GLES2Util::ComputeImageGroupSize(format, type);
    pixels = reinterpret_cast<const int8*>(pixels) +
        unpack_skip_pixels_ * group_size;
  }

  ScopedTransferBufferPtr buffer(temp_size, helper_, transfer_buffer_);
  TexSubImage2DImpl(
      target, level, xoffset, yoffset, width, height, format, type,
      unpadded_row_size, pixels, src_padded_row_size, GL_FALSE, &buffer,
      padded_row_size);
}

static GLint ComputeNumRowsThatFitInBuffer(
    GLsizeiptr padded_row_size, GLsizeiptr unpadded_row_size,
    unsigned int size) {
  GPU_DCHECK_GE(unpadded_row_size, 0);
  if (padded_row_size == 0) {
    return 1;
  }
  GLint num_rows = size / padded_row_size;
  return num_rows + (size - num_rows * padded_row_size) / unpadded_row_size;
}

void GLES2Implementation::TexSubImage2DImpl(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, uint32 unpadded_row_size,
    const void* pixels, uint32 pixels_padded_row_size, GLboolean internal,
    ScopedTransferBufferPtr* buffer, uint32 buffer_padded_row_size) {
  GPU_DCHECK(buffer);
  GPU_DCHECK_GE(level, 0);
  GPU_DCHECK_GT(height, 0);
  GPU_DCHECK_GT(width, 0);

  const int8* source = reinterpret_cast<const int8*>(pixels);
  GLint original_yoffset = yoffset;
  // Transfer by rows.
  while (height) {
    unsigned int desired_size =
        buffer_padded_row_size * (height - 1) + unpadded_row_size;
    if (!buffer->valid() || buffer->size() == 0) {
      buffer->Reset(desired_size);
      if (!buffer->valid()) {
        return;
      }
    }

    GLint num_rows = ComputeNumRowsThatFitInBuffer(
        buffer_padded_row_size, unpadded_row_size, buffer->size());
    num_rows = std::min(num_rows, height);
    CopyRectToBuffer(
        source, num_rows, unpadded_row_size, pixels_padded_row_size,
        unpack_flip_y_, buffer->address(), buffer_padded_row_size);
    GLint y = unpack_flip_y_ ? original_yoffset + height - num_rows : yoffset;
    helper_->TexSubImage2D(
        target, level, xoffset, y, width, num_rows, format, type,
        buffer->shm_id(), buffer->offset(), internal);
    buffer->Release();
    yoffset += num_rows;
    source += num_rows * pixels_padded_row_size;
    height -= num_rows;
  }
}

bool GLES2Implementation::GetActiveAttribHelper(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  // Clear the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  typedef gles2::GetActiveAttrib::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return false;
  }
  // Set as failed so if the command fails we'll recover.
  result->success = false;
  helper_->GetActiveAttrib(program, index, kResultBucketId,
                           GetResultShmId(), GetResultShmOffset());
  WaitForCmd();
  if (result->success) {
    if (size) {
      *size = result->size;
    }
    if (type) {
      *type = result->type;
    }
    if (length || name) {
      std::vector<int8> str;
      GetBucketContents(kResultBucketId, &str);
      GLsizei max_size = std::min(static_cast<size_t>(bufsize) - 1,
                                  std::max(static_cast<size_t>(0),
                                           str.size() - 1));
      if (length) {
        *length = max_size;
      }
      if (name && bufsize > 0) {
        memcpy(name, &str[0], max_size);
        name[max_size] = '\0';
      }
    }
  }
  return result->success != 0;
}

void GLES2Implementation::GetActiveAttrib(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGetActiveAttrib("
      << program << ", " << index << ", " << bufsize << ", "
      << static_cast<const void*>(length) << ", "
      << static_cast<const void*>(size) << ", "
      << static_cast<const void*>(type) << ", "
      << static_cast<const void*>(name) << ", ");
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetActiveAttrib: bufsize < 0");
    return;
  }
  TRACE_EVENT0("gpu", "GLES2::GetActiveAttrib");
  bool success = share_group_->program_info_manager()->GetActiveAttrib(
        this, program, index, bufsize, length, size, type, name);
  if (success) {
    if (size) {
      GPU_CLIENT_LOG("  size: " << *size);
    }
    if (type) {
      GPU_CLIENT_LOG("  type: " << GLES2Util::GetStringEnum(*type));
    }
    if (name) {
      GPU_CLIENT_LOG("  name: " << name);
    }
  }
}

bool GLES2Implementation::GetActiveUniformHelper(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  // Clear the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  typedef gles2::GetActiveUniform::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return false;
  }
  // Set as failed so if the command fails we'll recover.
  result->success = false;
  helper_->GetActiveUniform(program, index, kResultBucketId,
                            GetResultShmId(), GetResultShmOffset());
  WaitForCmd();
  if (result->success) {
    if (size) {
      *size = result->size;
    }
    if (type) {
      *type = result->type;
    }
    if (length || name) {
      std::vector<int8> str;
      GetBucketContents(kResultBucketId, &str);
      GLsizei max_size = std::min(static_cast<size_t>(bufsize) - 1,
                                  std::max(static_cast<size_t>(0),
                                           str.size() - 1));
      if (length) {
        *length = max_size;
      }
      if (name && bufsize > 0) {
        memcpy(name, &str[0], max_size);
        name[max_size] = '\0';
      }
    }
  }
  return result->success != 0;
}

void GLES2Implementation::GetActiveUniform(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGetActiveUniform("
      << program << ", " << index << ", " << bufsize << ", "
      << static_cast<const void*>(length) << ", "
      << static_cast<const void*>(size) << ", "
      << static_cast<const void*>(type) << ", "
      << static_cast<const void*>(name) << ", ");
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetActiveUniform: bufsize < 0");
    return;
  }
  TRACE_EVENT0("gpu", "GLES2::GetActiveUniform");
  bool success = share_group_->program_info_manager()->GetActiveUniform(
      this, program, index, bufsize, length, size, type, name);
  if (success) {
    if (size) {
      GPU_CLIENT_LOG("  size: " << *size);
    }
    if (type) {
      GPU_CLIENT_LOG("  type: " << GLES2Util::GetStringEnum(*type));
    }
    if (name) {
      GPU_CLIENT_LOG("  name: " << name);
    }
  }
}

void GLES2Implementation::GetAttachedShaders(
    GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGetAttachedShaders("
      << program << ", " << maxcount << ", "
      << static_cast<const void*>(count) << ", "
      << static_cast<const void*>(shaders) << ", ");
  if (maxcount < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetAttachedShaders: maxcount < 0");
    return;
  }
  TRACE_EVENT0("gpu", "GLES2::GetAttachedShaders");
  typedef gles2::GetAttachedShaders::Result Result;
  uint32 size = Result::ComputeSize(maxcount);
  Result* result = static_cast<Result*>(transfer_buffer_->Alloc(size));
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetAttachedShaders(
    program,
    transfer_buffer_->GetShmId(),
    transfer_buffer_->GetOffset(result),
    size);
  int32 token = helper_->InsertToken();
  WaitForCmd();
  if (count) {
    *count = result->GetNumResults();
  }
  result->CopyResult(shaders);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  transfer_buffer_->FreePendingToken(result, token);
}

void GLES2Implementation::GetShaderPrecisionFormat(
    GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGetShaderPrecisionFormat("
      << GLES2Util::GetStringShaderType(shadertype) << ", "
      << GLES2Util::GetStringShaderPrecision(precisiontype) << ", "
      << static_cast<const void*>(range) << ", "
      << static_cast<const void*>(precision) << ", ");
  TRACE_EVENT0("gpu", "GLES2::GetShaderPrecisionFormat");
  typedef gles2::GetShaderPrecisionFormat::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return;
  }
  result->success = false;
  helper_->GetShaderPrecisionFormat(
    shadertype, precisiontype, GetResultShmId(), GetResultShmOffset());
  WaitForCmd();
  if (result->success) {
    if (range) {
      range[0] = result->min_range;
      range[1] = result->max_range;
      GPU_CLIENT_LOG("  min_range: " << range[0]);
      GPU_CLIENT_LOG("  min_range: " << range[1]);
    }
    if (precision) {
      precision[0] = result->precision;
      GPU_CLIENT_LOG("  min_range: " << precision[0]);
    }
  }
}

const GLubyte* GLES2Implementation::GetStringHelper(GLenum name) {
  const char* result = NULL;
  // Clears the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetString(name, kResultBucketId);
  std::string str;
  if (GetBucketAsString(kResultBucketId, &str)) {
    // Adds extensions implemented on client side only.
    switch (name) {
      case GL_EXTENSIONS:
        str += std::string(str.empty() ? "" : " ") +
            "GL_CHROMIUM_map_sub "
            "GL_CHROMIUM_flipy "
            "GL_EXT_unpack_subimage";
        break;
      default:
        break;
    }

    // Because of WebGL the extensions can change. We have to cache each unique
    // result since we don't know when the client will stop referring to a
    // previous one it queries.
    GLStringMap::iterator it = gl_strings_.find(name);
    if (it == gl_strings_.end()) {
      std::set<std::string> strings;
      std::pair<GLStringMap::iterator, bool> insert_result =
          gl_strings_.insert(std::make_pair(name, strings));
      GPU_DCHECK(insert_result.second);
      it = insert_result.first;
    }
    std::set<std::string>& string_set = it->second;
    std::set<std::string>::const_iterator sit = string_set.find(str);
    if (sit != string_set.end()) {
      result = sit->c_str();
    } else {
      std::pair<std::set<std::string>::const_iterator, bool> insert_result =
          string_set.insert(str);
      GPU_DCHECK(insert_result.second);
      result = insert_result.first->c_str();
    }
  }
  return reinterpret_cast<const GLubyte*>(result);
}

const GLubyte* GLES2Implementation::GetString(GLenum name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGetString("
      << GLES2Util::GetStringStringType(name) << ")");
  const GLubyte* result = GetStringHelper(name);
  GPU_CLIENT_LOG("  returned " << reinterpret_cast<const char*>(result));
  return result;
}

void GLES2Implementation::GetUniformfv(
    GLuint program, GLint location, GLfloat* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGetUniformfv("
      << program << ", " << location << ", "
      << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2::GetUniformfv");
  typedef gles2::GetUniformfv::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetUniformfv(
      program, location, GetResultShmId(), GetResultShmOffset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
}

void GLES2Implementation::GetUniformiv(
    GLuint program, GLint location, GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGetUniformiv("
      << program << ", " << location << ", "
      << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2::GetUniformiv");
  typedef gles2::GetUniformiv::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetUniformiv(
      program, location, GetResultShmId(), GetResultShmOffset());
  WaitForCmd();
  GetResultAs<gles2::GetUniformfv::Result*>()->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
}

void GLES2Implementation::ReadPixels(
    GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format,
    GLenum type, void* pixels) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glReadPixels("
      << xoffset << ", " << yoffset << ", "
      << width << ", " << height << ", "
      << GLES2Util::GetStringReadPixelFormat(format) << ", "
      << GLES2Util::GetStringPixelType(type) << ", "
      << static_cast<const void*>(pixels) << ")");
  if (width < 0 || height < 0) {
    SetGLError(GL_INVALID_VALUE, "glReadPixels: dimensions < 0");
    return;
  }
  if (width == 0 || height == 0) {
    return;
  }

  // glReadPixel pads the size of each row of pixels by an amount specified by
  // glPixelStorei. So, we have to take that into account both in the fact that
  // the pixels returned from the ReadPixel command will include that padding
  // and that when we copy the results to the user's buffer we need to not
  // write those padding bytes but leave them as they are.

  TRACE_EVENT0("gpu", "GLES2::ReadPixels");
  typedef gles2::ReadPixels::Result Result;

  int8* dest = reinterpret_cast<int8*>(pixels);
  uint32 temp_size;
  uint32 unpadded_row_size;
  uint32 padded_row_size;
  if (!GLES2Util::ComputeImageDataSizes(
      width, 2, format, type, pack_alignment_, &temp_size, &unpadded_row_size,
      &padded_row_size)) {
    SetGLError(GL_INVALID_VALUE, "glReadPixels: size too large.");
    return;
  }
  // Transfer by rows.
  // The max rows we can transfer.
  while (height) {
    GLsizei desired_size = padded_row_size * height - 1 + unpadded_row_size;
    ScopedTransferBufferPtr buffer(desired_size, helper_, transfer_buffer_);
    if (!buffer.valid()) {
      return;
    }
    GLint num_rows = ComputeNumRowsThatFitInBuffer(
        padded_row_size, unpadded_row_size, buffer.size());
    num_rows = std::min(num_rows, height);
    // NOTE: We must look up the address of the result area AFTER allocation
    // of the transfer buffer since the transfer buffer may be reallocated.
    Result* result = GetResultAs<Result*>();
    if (!result) {
      return;
    }
    *result = 0;  // mark as failed.
    helper_->ReadPixels(
        xoffset, yoffset, width, num_rows, format, type,
        buffer.shm_id(), buffer.offset(),
        GetResultShmId(), GetResultShmOffset());
    WaitForCmd();
    if (*result != 0) {
      // when doing a y-flip we have to iterate through top-to-bottom chunks
      // of the dst. The service side handles reversing the rows within a
      // chunk.
      int8* rows_dst;
      if (pack_reverse_row_order_) {
          rows_dst = dest + (height - num_rows) * padded_row_size;
      } else {
          rows_dst = dest;
      }
      // We have to copy 1 row at a time to avoid writing pad bytes.
      const int8* src = static_cast<const int8*>(buffer.address());
      for (GLint yy = 0; yy < num_rows; ++yy) {
        memcpy(rows_dst, src, unpadded_row_size);
        rows_dst += padded_row_size;
        src += padded_row_size;
      }
      if (!pack_reverse_row_order_) {
        dest = rows_dst;
      }
    }
    // If it was not marked as successful exit.
    if (*result == 0) {
      return;
    }
    yoffset += num_rows;
    height -= num_rows;
  }
}

void GLES2Implementation::ActiveTexture(GLenum texture) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glActiveTexture("
      << GLES2Util::GetStringEnum(texture) << ")");
  GLuint texture_index = texture - GL_TEXTURE0;
  if (texture_index >= static_cast<GLuint>(
      gl_state_.int_state.max_combined_texture_image_units)) {
    SetGLError(GL_INVALID_ENUM, "glActiveTexture: texture_unit out of range.");
    return;
  }

  active_texture_unit_ = texture_index;
  helper_->ActiveTexture(texture);
}

// NOTE #1: On old versions of OpenGL, calling glBindXXX with an unused id
// generates a new resource. On newer versions of OpenGL they don't. The code
// related to binding below will need to change if we switch to the new OpenGL
// model. Specifically it assumes a bind will succeed which is always true in
// the old model but possibly not true in the new model if another context has
// deleted the resource.

void GLES2Implementation::BindBufferHelper(
    GLenum target, GLuint buffer) {
  // TODO(gman): See note #1 above.
  switch (target) {
    case GL_ARRAY_BUFFER:
      bound_array_buffer_id_ = buffer;
      break;
    case GL_ELEMENT_ARRAY_BUFFER:
      bound_element_array_buffer_id_ = buffer;
      break;
    default:
      break;
  }
  // TODO(gman): There's a bug here. If the target is invalid the ID will not be
  // used even though it's marked it as used here.
  GetIdHandler(id_namespaces::kBuffers)->MarkAsUsedForBind(buffer);
}

void GLES2Implementation::BindFramebufferHelper(
    GLenum target, GLuint framebuffer) {
  // TODO(gman): See note #1 above.
  switch (target) {
    case GL_FRAMEBUFFER:
      bound_framebuffer_ = framebuffer;
      break;
    default:
      break;
  }
  // TODO(gman): There's a bug here. If the target is invalid the ID will not be
  // used even though it's marked it as used here.
  GetIdHandler(id_namespaces::kFramebuffers)->MarkAsUsedForBind(framebuffer);
}

void GLES2Implementation::BindRenderbufferHelper(
    GLenum target, GLuint renderbuffer) {
  // TODO(gman): See note #1 above.
  switch (target) {
    case GL_RENDERBUFFER:
      bound_renderbuffer_ = renderbuffer;
      break;
    default:
      break;
  }
  // TODO(gman): There's a bug here. If the target is invalid the ID will not be
  // used even though it's marked it as used here.
  GetIdHandler(id_namespaces::kRenderbuffers)->MarkAsUsedForBind(renderbuffer);
}

void GLES2Implementation::BindTextureHelper(GLenum target, GLuint texture) {
  // TODO(gman): See note #1 above.
  TextureUnit& unit = texture_units_[active_texture_unit_];
  switch (target) {
    case GL_TEXTURE_2D:
      unit.bound_texture_2d = texture;
      break;
    case GL_TEXTURE_CUBE_MAP:
      unit.bound_texture_cube_map = texture;
      break;
    default:
      break;
  }
  // TODO(gman): There's a bug here. If the target is invalid the ID will not be
  // used. even though it's marked it as used here.
  GetIdHandler(id_namespaces::kTextures)->MarkAsUsedForBind(texture);
}

#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
bool GLES2Implementation::IsBufferReservedId(GLuint id) {
  for (size_t ii = 0; ii < arraysize(reserved_ids_); ++ii) {
    if (id == reserved_ids_[ii]) {
      return true;
    }
  }
  return false;
}
#else
bool GLES2Implementation::IsBufferReservedId(GLuint /* id */) {
  return false;
}
#endif

void GLES2Implementation::DeleteBuffersHelper(
    GLsizei n, const GLuint* buffers) {
  if (!GetIdHandler(id_namespaces::kBuffers)->FreeIds(
      this, n, buffers, &GLES2Implementation::DeleteBuffersStub)) {
    SetGLError(
        GL_INVALID_VALUE,
        "glDeleteBuffers: id not created by this context.");
    return;
  }
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (buffers[ii] == bound_array_buffer_id_) {
      bound_array_buffer_id_ = 0;
    }
    if (buffers[ii] == bound_element_array_buffer_id_) {
      bound_element_array_buffer_id_ = 0;
    }
  }
}

void GLES2Implementation::DeleteBuffersStub(
    GLsizei n, const GLuint* buffers) {
  helper_->DeleteBuffersImmediate(n, buffers);
}


void GLES2Implementation::DeleteFramebuffersHelper(
    GLsizei n, const GLuint* framebuffers) {
  if (!GetIdHandler(id_namespaces::kFramebuffers)->FreeIds(
      this, n, framebuffers, &GLES2Implementation::DeleteFramebuffersStub)) {
    SetGLError(
        GL_INVALID_VALUE,
        "glDeleteFramebuffers: id not created by this context.");
    return;
  }
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (framebuffers[ii] == bound_framebuffer_) {
      bound_framebuffer_ = 0;
    }
  }
}

void GLES2Implementation::DeleteFramebuffersStub(
    GLsizei n, const GLuint* framebuffers) {
  helper_->DeleteFramebuffersImmediate(n, framebuffers);
}

void GLES2Implementation::DeleteRenderbuffersHelper(
    GLsizei n, const GLuint* renderbuffers) {
  if (!GetIdHandler(id_namespaces::kRenderbuffers)->FreeIds(
      this, n, renderbuffers, &GLES2Implementation::DeleteRenderbuffersStub)) {
    SetGLError(
        GL_INVALID_VALUE,
        "glDeleteRenderbuffers: id not created by this context.");
    return;
  }
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (renderbuffers[ii] == bound_renderbuffer_) {
      bound_renderbuffer_ = 0;
    }
  }
}

void GLES2Implementation::DeleteRenderbuffersStub(
    GLsizei n, const GLuint* renderbuffers) {
  helper_->DeleteRenderbuffersImmediate(n, renderbuffers);
}

void GLES2Implementation::DeleteTexturesHelper(
    GLsizei n, const GLuint* textures) {
  if (!GetIdHandler(id_namespaces::kTextures)->FreeIds(
      this, n, textures, &GLES2Implementation::DeleteTexturesStub)) {
    SetGLError(
        GL_INVALID_VALUE,
        "glDeleteTextures: id not created by this context.");
    return;
  }
  for (GLsizei ii = 0; ii < n; ++ii) {
    for (GLint tt = 0;
         tt < gl_state_.int_state.max_combined_texture_image_units;
         ++tt) {
      TextureUnit& unit = texture_units_[tt];
      if (textures[ii] == unit.bound_texture_2d) {
        unit.bound_texture_2d = 0;
      }
      if (textures[ii] == unit.bound_texture_cube_map) {
        unit.bound_texture_cube_map = 0;
      }
    }
  }
}

void GLES2Implementation::DeleteTexturesStub(
    GLsizei n, const GLuint* textures) {
  helper_->DeleteTexturesImmediate(n, textures);
}

void GLES2Implementation::DisableVertexAttribArray(GLuint index) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << this << "] glDisableVertexAttribArray(" << index << ")");
#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  client_side_buffer_helper_->SetAttribEnable(index, false);
#endif
  helper_->DisableVertexAttribArray(index);
}

void GLES2Implementation::EnableVertexAttribArray(GLuint index) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glEnableVertexAttribArray(" << index << ")");
#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  client_side_buffer_helper_->SetAttribEnable(index, true);
#endif
  helper_->EnableVertexAttribArray(index);
}

void GLES2Implementation::DrawArrays(GLenum mode, GLint first, GLsizei count) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glDrawArrays("
      << GLES2Util::GetStringDrawMode(mode) << ", "
      << first << ", " << count << ")");
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glDrawArrays: count < 0");
    return;
  }
#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  bool have_client_side =
      client_side_buffer_helper_->HaveEnabledClientSideBuffers();
  if (have_client_side) {
    client_side_buffer_helper_->SetupSimulatedClientSideBuffers(
        this, helper_, first + count, 0);
  }
#endif
  helper_->DrawArrays(mode, first, count);
#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  if (have_client_side) {
    // Restore the user's current binding.
    helper_->BindBuffer(GL_ARRAY_BUFFER, bound_array_buffer_id_);
  }
#endif
}

#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
bool GLES2Implementation::GetVertexAttribHelper(
    GLuint index, GLenum pname, uint32* param) {
  const ClientSideBufferHelper::VertexAttribInfo* info =
      client_side_buffer_helper_->GetAttribInfo(index);
  if (!info) {
    return false;
  }

  switch (pname) {
    case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
      *param = info->buffer_id();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
      *param = info->enabled();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_SIZE:
      *param = info->size();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
      *param = info->stride();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_TYPE:
      *param = info->type();
      break;
    case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
      *param = info->normalized();
      break;
    case GL_CURRENT_VERTEX_ATTRIB:
      return false;  // pass through to service side.
    default:
      SetGLError(GL_INVALID_ENUM, "glGetVertexAttrib: invalid enum");
      break;
  }
  return true;
}
#endif  // GLES2_SUPPORT_CLIENT_SIDE_ARRAYS

void GLES2Implementation::GetVertexAttribfv(
    GLuint index, GLenum pname, GLfloat* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGetVertexAttribfv("
      << index << ", "
      << GLES2Util::GetStringVertexAttribute(pname) << ", "
      << static_cast<const void*>(params) << ")");
#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  uint32 value = 0;
  if (GetVertexAttribHelper(index, pname, &value)) {
    *params = static_cast<float>(value);
    return;
  }
#endif
  TRACE_EVENT0("gpu", "GLES2::GetVertexAttribfv");
  typedef GetVertexAttribfv::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetVertexAttribfv(
      index, pname, GetResultShmId(), GetResultShmOffset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
}

void GLES2Implementation::GetVertexAttribiv(
    GLuint index, GLenum pname, GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGetVertexAttribiv("
      << index << ", "
      << GLES2Util::GetStringVertexAttribute(pname) << ", "
      << static_cast<const void*>(params) << ")");
#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  uint32 value = 0;
  if (GetVertexAttribHelper(index, pname, &value)) {
    *params = value;
    return;
  }
#endif
  TRACE_EVENT0("gpu", "GLES2::GetVertexAttribiv");
  typedef GetVertexAttribiv::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetVertexAttribiv(
      index, pname, GetResultShmId(), GetResultShmOffset());
  WaitForCmd();
  result->CopyResult(params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32 i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
}

GLboolean GLES2Implementation::EnableFeatureCHROMIUM(
    const char* feature) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glEnableFeatureCHROMIUM("
                 << feature << ")");
  TRACE_EVENT0("gpu", "GLES2::EnableFeatureCHROMIUM");
  typedef EnableFeatureCHROMIUM::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return false;
  }
  *result = 0;
  SetBucketAsCString(kResultBucketId, feature);
  helper_->EnableFeatureCHROMIUM(
      kResultBucketId, GetResultShmId(), GetResultShmOffset());
  WaitForCmd();
  helper_->SetBucketSize(kResultBucketId, 0);
  GPU_CLIENT_LOG("   returned " << GLES2Util::GetStringBool(*result));
  return *result;
}

void* GLES2Implementation::MapBufferSubDataCHROMIUM(
    GLuint target, GLintptr offset, GLsizeiptr size, GLenum access) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glMapBufferSubDataCHROMIUM("
      << target << ", " << offset << ", " << size << ", "
      << GLES2Util::GetStringEnum(access) << ")");
  // NOTE: target is NOT checked because the service will check it
  // and we don't know what targets are valid.
  if (access != GL_WRITE_ONLY) {
    SetGLError(GL_INVALID_ENUM, "MapBufferSubDataCHROMIUM: bad access mode");
    return NULL;
  }
  if (offset < 0 || size < 0) {
    SetGLError(GL_INVALID_VALUE, "MapBufferSubDataCHROMIUM: bad range");
    return NULL;
  }
  int32 shm_id;
  unsigned int shm_offset;
  void* mem = mapped_memory_->Alloc(size, &shm_id, &shm_offset);
  if (!mem) {
    SetGLError(GL_OUT_OF_MEMORY, "MapBufferSubDataCHROMIUM: out of memory");
    return NULL;
  }

  std::pair<MappedBufferMap::iterator, bool> result =
     mapped_buffers_.insert(std::make_pair(
         mem,
         MappedBuffer(
             access, shm_id, mem, shm_offset, target, offset, size)));
  GPU_DCHECK(result.second);
  GPU_CLIENT_LOG("  returned " << mem);
  return mem;
}

void GLES2Implementation::UnmapBufferSubDataCHROMIUM(const void* mem) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << this << "] glUnmapBufferSubDataCHROMIUM(" << mem << ")");
  MappedBufferMap::iterator it = mapped_buffers_.find(mem);
  if (it == mapped_buffers_.end()) {
    SetGLError(
        GL_INVALID_VALUE, "UnmapBufferSubDataCHROMIUM: buffer not mapped");
    return;
  }
  const MappedBuffer& mb = it->second;
  helper_->BufferSubData(
      mb.target, mb.offset, mb.size, mb.shm_id, mb.shm_offset);
  mapped_memory_->FreePendingToken(mb.shm_memory, helper_->InsertToken());
  // Flushing after unmap lets the service side start processing commands
  // sooner. However, on lowend devices, the thread thrashing causes is
  // worse than the latency hit.
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  helper_->CommandBufferHelper::Flush();
#endif
  mapped_buffers_.erase(it);
}

void* GLES2Implementation::MapTexSubImage2DCHROMIUM(
     GLenum target,
     GLint level,
     GLint xoffset,
     GLint yoffset,
     GLsizei width,
     GLsizei height,
     GLenum format,
     GLenum type,
     GLenum access) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glMapTexSubImage2DCHROMIUM("
      << target << ", " << level << ", "
      << xoffset << ", " << yoffset << ", "
      << width << ", " << height << ", "
      << GLES2Util::GetStringTextureFormat(format) << ", "
      << GLES2Util::GetStringPixelType(type) << ", "
      << GLES2Util::GetStringEnum(access) << ")");
  if (access != GL_WRITE_ONLY) {
    SetGLError(GL_INVALID_ENUM, "MapTexSubImage2DCHROMIUM: bad access mode");
    return NULL;
  }
  // NOTE: target is NOT checked because the service will check it
  // and we don't know what targets are valid.
  if (level < 0 || xoffset < 0 || yoffset < 0 || width < 0 || height < 0) {
    SetGLError(GL_INVALID_VALUE, "MapTexSubImage2DCHROMIUM: bad dimensions");
    return NULL;
  }
  uint32 size;
  if (!GLES2Util::ComputeImageDataSizes(
      width, height, format, type, unpack_alignment_, &size, NULL, NULL)) {
    SetGLError(
        GL_INVALID_VALUE, "MapTexSubImage2DCHROMIUM: image size too large");
    return NULL;
  }
  int32 shm_id;
  unsigned int shm_offset;
  void* mem = mapped_memory_->Alloc(size, &shm_id, &shm_offset);
  if (!mem) {
    SetGLError(GL_OUT_OF_MEMORY, "MapTexSubImage2DCHROMIUM: out of memory");
    return NULL;
  }

  std::pair<MappedTextureMap::iterator, bool> result =
     mapped_textures_.insert(std::make_pair(
         mem,
         MappedTexture(
             access, shm_id, mem, shm_offset,
             target, level, xoffset, yoffset, width, height, format, type)));
  GPU_DCHECK(result.second);
  GPU_CLIENT_LOG("  returned " << mem);
  return mem;
}

void GLES2Implementation::UnmapTexSubImage2DCHROMIUM(const void* mem) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << this << "] glUnmapTexSubImage2DCHROMIUM(" << mem << ")");
  MappedTextureMap::iterator it = mapped_textures_.find(mem);
  if (it == mapped_textures_.end()) {
    SetGLError(
        GL_INVALID_VALUE, "UnmapTexSubImage2DCHROMIUM: texture not mapped");
    return;
  }
  const MappedTexture& mt = it->second;
  helper_->TexSubImage2D(
      mt.target, mt.level, mt.xoffset, mt.yoffset, mt.width, mt.height,
      mt.format, mt.type, mt.shm_id, mt.shm_offset, GL_FALSE);
  mapped_memory_->FreePendingToken(mt.shm_memory, helper_->InsertToken());
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  helper_->CommandBufferHelper::Flush();
#endif
  mapped_textures_.erase(it);
}

void GLES2Implementation::ResizeCHROMIUM(GLuint width, GLuint height) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glResizeCHROMIUM("
                 << width << ", " << height << ")");
  helper_->ResizeCHROMIUM(width, height);
}

const GLchar* GLES2Implementation::GetRequestableExtensionsCHROMIUM() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGetRequestableExtensionsCHROMIUM()");
  TRACE_EVENT0("gpu",
               "GLES2Implementation::GetRequestableExtensionsCHROMIUM()");
  const char* result = NULL;
  // Clear the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetRequestableExtensionsCHROMIUM(kResultBucketId);
  std::string str;
  if (GetBucketAsString(kResultBucketId, &str)) {
    // The set of requestable extensions shrinks as we enable
    // them. Because we don't know when the client will stop referring
    // to a previous one it queries (see GetString) we need to cache
    // the unique results.
    std::set<std::string>::const_iterator sit =
        requestable_extensions_set_.find(str);
    if (sit != requestable_extensions_set_.end()) {
      result = sit->c_str();
    } else {
      std::pair<std::set<std::string>::const_iterator, bool> insert_result =
          requestable_extensions_set_.insert(str);
      GPU_DCHECK(insert_result.second);
      result = insert_result.first->c_str();
    }
  }
  GPU_CLIENT_LOG("  returned " << result);
  return reinterpret_cast<const GLchar*>(result);
}

void GLES2Implementation::RequestExtensionCHROMIUM(const char* extension) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glRequestExtensionCHROMIUM("
                 << extension << ")");
  SetBucketAsCString(kResultBucketId, extension);
  helper_->RequestExtensionCHROMIUM(kResultBucketId);
  helper_->SetBucketSize(kResultBucketId, 0);
  if (kUnavailableExtensionStatus == angle_pack_reverse_row_order_status &&
      !strcmp(extension, "GL_ANGLE_pack_reverse_row_order")) {
    angle_pack_reverse_row_order_status = kUnknownExtensionStatus;
  }
}

void GLES2Implementation::RateLimitOffscreenContextCHROMIUM() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glRateLimitOffscreenCHROMIUM()");
  // Wait if this would add too many rate limit tokens.
  if (rate_limit_tokens_.size() == kMaxSwapBuffers) {
    helper_->WaitForToken(rate_limit_tokens_.front());
    rate_limit_tokens_.pop();
  }
  rate_limit_tokens_.push(helper_->InsertToken());
}

void GLES2Implementation::GetMultipleIntegervCHROMIUM(
    const GLenum* pnames, GLuint count, GLint* results, GLsizeiptr size) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGetMultipleIntegervCHROMIUM("
                 << static_cast<const void*>(pnames) << ", "
                 << count << ", " << results << ", " << size << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLuint i = 0; i < count; ++i) {
      GPU_CLIENT_LOG(
          "  " << i << ": " << GLES2Util::GetStringGLState(pnames[i]));
    }
  });
  int num_results = 0;
  for (GLuint ii = 0; ii < count; ++ii) {
    int num = util_.GLGetNumValuesReturned(pnames[ii]);
    if (!num) {
      SetGLError(GL_INVALID_ENUM, "glGetMultipleIntegervCHROMIUM: bad pname");
      return;
    }
    num_results += num;
  }
  if (static_cast<size_t>(size) != num_results * sizeof(GLint)) {
    SetGLError(GL_INVALID_VALUE, "glGetMultipleIntegervCHROMIUM: bad size");
    return;
  }
  for (int ii = 0; ii < num_results; ++ii) {
    if (results[ii] != 0) {
      SetGLError(GL_INVALID_VALUE,
                 "glGetMultipleIntegervCHROMIUM: results not set to zero.");
      return;
    }
  }
  uint32 size_needed =
      count * sizeof(pnames[0]) + num_results * sizeof(results[0]);
  void* buffer = transfer_buffer_->Alloc(size_needed);
  if (!buffer) {
    return;
  }
  GLenum* pnames_buffer = static_cast<GLenum*>(buffer);
  void* results_buffer = pnames_buffer + count;
  memcpy(pnames_buffer, pnames, count * sizeof(GLenum));
  memset(results_buffer, 0, num_results * sizeof(GLint));
  helper_->GetMultipleIntegervCHROMIUM(
      transfer_buffer_->GetShmId(),
      transfer_buffer_->GetOffset(pnames_buffer),
      count,
      transfer_buffer_->GetShmId(),
      transfer_buffer_->GetOffset(results_buffer),
      size);
  WaitForCmd();
  memcpy(results, results_buffer, size);
  // TODO(gman): We should be able to free without a token.
  transfer_buffer_->FreePendingToken(buffer, helper_->InsertToken());
  GPU_CLIENT_LOG("  returned");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int i = 0; i < num_results; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << (results[i]));
    }
  });
}

void GLES2Implementation::GetProgramInfoCHROMIUMHelper(
    GLuint program, std::vector<int8>* result) {
  GPU_DCHECK(result);
  // Clear the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetProgramInfoCHROMIUM(program, kResultBucketId);
  GetBucketContents(kResultBucketId, result);
}

void GLES2Implementation::GetProgramInfoCHROMIUM(
    GLuint program, GLsizei bufsize, GLsizei* size, void* info) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE, "glProgramInfoCHROMIUM: bufsize less than 0.");
    return;
  }
  if (size == NULL) {
    SetGLError(GL_INVALID_VALUE, "glProgramInfoCHROMIUM: size is null.");
    return;
  }
  // Make sure they've set size to 0 else the value will be undefined on
  // lost context.
  GPU_DCHECK(*size == 0);
  std::vector<int8> result;
  GetProgramInfoCHROMIUMHelper(program, &result);
  if (result.empty()) {
    return;
  }
  *size = result.size();
  if (!info) {
    return;
  }
  if (static_cast<size_t>(bufsize) < result.size()) {
    SetGLError(GL_INVALID_OPERATION,
               "glProgramInfoCHROMIUM: bufsize is too small for result.");
    return;
  }
  memcpy(info, &result[0], result.size());
}

GLuint GLES2Implementation::CreateStreamTextureCHROMIUM(GLuint texture) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] CreateStreamTextureCHROMIUM("
      << texture << ")");
  TRACE_EVENT0("gpu", "GLES2::CreateStreamTextureCHROMIUM");
  typedef CreateStreamTextureCHROMIUM::Result Result;
  Result* result = GetResultAs<Result*>();
  if (!result) {
    return GL_ZERO;
  }
  *result = GL_ZERO;

  helper_->CreateStreamTextureCHROMIUM(texture,
                                       GetResultShmId(),
                                       GetResultShmOffset());
  WaitForCmd();

  return *result;
}

void GLES2Implementation::DestroyStreamTextureCHROMIUM(GLuint texture) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] DestroyStreamTextureCHROMIUM("
      << texture << ")");
  TRACE_EVENT0("gpu", "GLES2::DestroyStreamTextureCHROMIUM");
  helper_->DestroyStreamTextureCHROMIUM(texture);
}

void GLES2Implementation::PostSubBufferCHROMIUM(
    GLint x, GLint y, GLint width, GLint height) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] PostSubBufferCHROMIUM("
      << x << ", " << y << ", " << width << ", " << height << ")");
  TRACE_EVENT0("gpu", "GLES2::PostSubBufferCHROMIUM");

  // Same flow control as GLES2Implementation::SwapBuffers (see comments there).
  swap_buffers_tokens_.push(helper_->InsertToken());
  helper_->PostSubBufferCHROMIUM(x, y, width, height);
  helper_->CommandBufferHelper::Flush();
  if (swap_buffers_tokens_.size() > kMaxSwapBuffers + 1) {
    helper_->WaitForToken(swap_buffers_tokens_.front());
    swap_buffers_tokens_.pop();
  }
}

void GLES2Implementation::DeleteQueriesEXTHelper(
    GLsizei n, const GLuint* queries) {
  // TODO(gman): Remove this as queries are not shared resources.
  if (!GetIdHandler(id_namespaces::kQueries)->FreeIds(
      this, n, queries, &GLES2Implementation::DeleteQueriesStub)) {
    SetGLError(
        GL_INVALID_VALUE,
        "glDeleteTextures: id not created by this context.");
    return;
  }
  // When you delete a query you can't mark its memory as unused until it's
  // completed.
  // Note: If you don't do this you won't mess up the service but you will mess
  // up yourself.

  // TODO(gman): Consider making this faster by putting pending quereies
  // on some queue to be removed when they are finished.
  bool query_pending = false;
  for (GLsizei ii = 0; ii < n; ++ii) {
    QueryTracker::Query* query = query_tracker_->GetQuery(queries[ii]);
    if (query && query->Pending()) {
      query_pending = true;
      break;
    }
  }

  if (query_pending) {
    Finish();
  }

  for (GLsizei ii = 0; ii < n; ++ii) {
    QueryTracker::Query* query = query_tracker_->GetQuery(queries[ii]);
    if (query && query->Pending()) {
      GPU_CHECK(!query->CheckResultsAvailable(helper_));
    }
    query_tracker_->RemoveQuery(queries[ii]);
  }
  helper_->DeleteQueriesEXTImmediate(n, queries);
}

// TODO(gman): Remove this. Queries are not shared resources.
void GLES2Implementation::DeleteQueriesStub(
    GLsizei /* n */, const GLuint* /* queries */) {
}

GLboolean GLES2Implementation::IsQueryEXT(GLuint id) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] IsQueryEXT(" << id << ")");

  // TODO(gman): To be spec compliant IDs from other contexts sharing
  // resources need to return true here even though you can't share
  // queries across contexts?
  return query_tracker_->GetQuery(id) != NULL;
}

void GLES2Implementation::BeginQueryEXT(GLenum target, GLuint id) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] BeginQueryEXT("
                 << GLES2Util::GetStringQueryTarget(target)
                 << ", " << id << ")");

  // if any outstanding queries INV_OP
  if (current_query_) {
    SetGLError(
        GL_INVALID_OPERATION, "glBeginQueryEXT: query already in progress");
    return;
  }

  // id = 0 INV_OP
  if (id == 0) {
    SetGLError(GL_INVALID_OPERATION, "glBeginQueryEXT: id is 0");
    return;
  }

  // TODO(gman) if id not GENned INV_OPERATION

  // if id does not have an object
  QueryTracker::Query* query = query_tracker_->GetQuery(id);
  if (!query) {
    query = query_tracker_->CreateQuery(id, target);
  } else if (query->target() != target) {
    SetGLError(GL_INVALID_OPERATION, "glBeginQueryEXT: target does not match");
    return;
  }

  current_query_ = query;

  // init memory, inc count
  query->MarkAsActive();

  // tell service about id, shared memory and count
  helper_->BeginQueryEXT(target, id, query->shm_id(), query->shm_offset());
}

void GLES2Implementation::EndQueryEXT(GLenum target) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] EndQueryEXT("
                 << GLES2Util::GetStringQueryTarget(target) << ")");

  if (!current_query_) {
    SetGLError(GL_INVALID_OPERATION, "glEndQueryEXT: no active query");
    return;
  }

  if (current_query_->target() != target) {
    SetGLError(GL_INVALID_OPERATION,
               "glEndQueryEXT: target does not match active query");
    return;
  }

  helper_->EndQueryEXT(target, current_query_->submit_count());
  current_query_->MarkAsPending(helper_->InsertToken());
  current_query_ = NULL;
}

void GLES2Implementation::GetQueryivEXT(
    GLenum target, GLenum pname, GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] GetQueryivEXT("
                 << GLES2Util::GetStringQueryTarget(target) << ", "
                 << GLES2Util::GetStringQueryParameter(pname) << ", "
                 << static_cast<const void*>(params) << ")");

  if (pname != GL_CURRENT_QUERY_EXT) {
    SetGLError(GL_INVALID_ENUM, "glGetQueryivEXT: invalid pname");
    return;
  }
  *params = (current_query_ && current_query_->target() == target) ?
      current_query_->id() : 0;
  GPU_CLIENT_LOG("  " << *params);
}

void GLES2Implementation::GetQueryObjectuivEXT(
    GLuint id, GLenum pname, GLuint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] GetQueryivEXT(" << id << ", "
                 << GLES2Util::GetStringQueryObjectParameter(pname) << ", "
                 << static_cast<const void*>(params) << ")");

  QueryTracker::Query* query = query_tracker_->GetQuery(id);
  if (!query) {
    SetGLError(GL_INVALID_OPERATION, "glQueryObjectuivEXT: unknown query id");
    return;
  }

  if (query == current_query_) {
    SetGLError(
        GL_INVALID_OPERATION,
        "glQueryObjectuivEXT: query active. Did you to call glEndQueryEXT?");
    return;
  }

  if (query->NeverUsed()) {
    SetGLError(
        GL_INVALID_OPERATION,
        "glQueryObjectuivEXT: Never used. Did you call glBeginQueryEXT?");
    return;
  }

  switch (pname) {
    case GL_QUERY_RESULT_EXT:
      if (!query->CheckResultsAvailable(helper_)) {
        helper_->WaitForToken(query->token());
        if (!query->CheckResultsAvailable(helper_)) {
          // TODO(gman): Speed this up.
          Finish();
          GPU_CHECK(query->CheckResultsAvailable(helper_));
        }
      }
      *params = query->GetResult();
      break;
    case GL_QUERY_RESULT_AVAILABLE_EXT:
      *params = query->CheckResultsAvailable(helper_);
      break;
    default:
      SetGLError(GL_INVALID_ENUM, "glQueryObjectuivEXT: unknown pname");
      break;
  }
  GPU_CLIENT_LOG("  " << *params);
}

void GLES2Implementation::DrawArraysInstancedANGLE(
    GLenum mode, GLint first, GLsizei count, GLsizei primcount) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glDrawArraysInstancedANGLE("
      << GLES2Util::GetStringDrawMode(mode) << ", "
      << first << ", " << count << ", " << primcount << ")");
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glDrawArraysInstancedANGLE: count < 0");
    return;
  }
  if (primcount < 0) {
    SetGLError(GL_INVALID_VALUE, "glDrawArraysInstancedANGLE: primcount < 0");
    return;
  }
  if (primcount == 0) {
    return;
  }
#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  bool have_client_side =
      client_side_buffer_helper_->HaveEnabledClientSideBuffers();
  if (have_client_side) {
    client_side_buffer_helper_->SetupSimulatedClientSideBuffers(
        this, helper_, first + count, primcount);
  }
#endif
  helper_->DrawArraysInstancedANGLE(mode, first, count, primcount);
#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  if (have_client_side) {
    // Restore the user's current binding.
    helper_->BindBuffer(GL_ARRAY_BUFFER, bound_array_buffer_id_);
  }
#endif
}

void GLES2Implementation::DrawElementsInstancedANGLE(
    GLenum mode, GLsizei count, GLenum type, const void* indices,
    GLsizei primcount) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glDrawElementsInstancedANGLE("
      << GLES2Util::GetStringDrawMode(mode) << ", "
      << count << ", "
      << GLES2Util::GetStringIndexType(type) << ", "
      << static_cast<const void*>(indices) << ", "
      << primcount << ")");
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE,
               "glDrawElementsInstancedANGLE: count less than 0.");
    return;
  }
  if (count == 0) {
    return;
  }
  if (primcount < 0) {
    SetGLError(GL_INVALID_VALUE,
               "glDrawElementsInstancedANGLE: primcount < 0");
    return;
  }
  if (primcount == 0) {
    return;
  }
#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  bool have_client_side =
      client_side_buffer_helper_->HaveEnabledClientSideBuffers();
  GLsizei num_elements = 0;
  GLuint offset = ToGLuint(indices);
  if (bound_element_array_buffer_id_ == 0) {
    // Index buffer is client side array.
    // Copy to buffer, scan for highest index.
    num_elements = client_side_buffer_helper_->SetupSimulatedIndexBuffer(
        this, helper_, count, type, indices);
    offset = 0;
  } else {
    // Index buffer is GL buffer. Ask the service for the highest vertex
    // that will be accessed. Note: It doesn't matter if another context
    // changes the contents of any of the buffers. The service will still
    // validate the indices. We just need to know how much to copy across.
    if (have_client_side) {
      num_elements = GetMaxValueInBufferCHROMIUMHelper(
          bound_element_array_buffer_id_, count, type, ToGLuint(indices)) + 1;
    }
  }
  if (have_client_side) {
    client_side_buffer_helper_->SetupSimulatedClientSideBuffers(
        this, helper_, num_elements, primcount);
  }
  helper_->DrawElementsInstancedANGLE(mode, count, type, offset, primcount);
  if (have_client_side) {
    // Restore the user's current binding.
    helper_->BindBuffer(GL_ARRAY_BUFFER, bound_array_buffer_id_);
  }
  if (bound_element_array_buffer_id_ == 0) {
    // Restore the element array binding.
    helper_->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  }
#else
  helper_->DrawElementsInstancedANGLE(
      mode, count, type, ToGLuint(indices), primcount);
#endif
}

void GLES2Implementation::GenMailboxCHROMIUM(
    GLbyte* mailbox) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << this << "] glGenMailboxCHROMIUM("
      << static_cast<const void*>(mailbox) << ")");
  TRACE_EVENT0("gpu", "GLES2::GenMailboxCHROMIUM");

  helper_->GenMailboxCHROMIUM(kResultBucketId);

  std::vector<GLbyte> result;
  GetBucketContents(kResultBucketId, &result);

  std::copy(result.begin(), result.end(), mailbox);
}

}  // namespace gles2
}  // namespace gpu
