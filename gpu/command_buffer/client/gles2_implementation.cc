// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class to emluate GLES2 over command buffers.

#include "../client/gles2_implementation.h"
#include "../common/gles2_cmd_utils.h"

namespace gpu {
namespace gles2 {

// A 32-bit and 64-bit compatible way of converting a pointer to a GLuint.
static GLuint ToGLuint(const void* ptr) {
  return static_cast<GLuint>(reinterpret_cast<size_t>(ptr));
}

#if defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)

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
          size_(0),
          type_(0),
          normalized_(GL_FALSE),
          pointer_(NULL),
          gl_stride_(0) {
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
  void SetupSimualtedClientSideBuffers(
      GLES2Implementation* gl,
      GLES2CmdHelper* gl_helper,
      GLsizei num_elements) {
    GLsizei total_size = 0;
    // Compute the size of the buffer we need.
    for (GLuint ii = 0; ii < max_vertex_attribs_; ++ii) {
      VertexAttribInfo& info = vertex_attrib_infos_[ii];
      if (info.IsClientSide() && info.enabled()) {
        size_t bytes_per_element =
            GLES2Util::GetGLTypeSizeForTexturesAndBuffers(info.type()) *
            info.size();
        total_size += RoundUpToMultipleOf4(
            bytes_per_element * num_elements);
      }
    }
    gl_helper->BindBuffer(GL_ARRAY_BUFFER, array_buffer_id_);
    array_buffer_offset_ = 0;
    if (total_size > array_buffer_size_) {
      gl->BufferData(GL_ARRAY_BUFFER, total_size, NULL, GL_DYNAMIC_DRAW);
      array_buffer_size_ = total_size;
    }
    for (GLuint ii = 0; ii < max_vertex_attribs_; ++ii) {
      VertexAttribInfo& info = vertex_attrib_infos_[ii];
      if (info.IsClientSide() && info.enabled()) {
        size_t bytes_per_element =
            GLES2Util::GetGLTypeSizeForTexturesAndBuffers(info.type()) *
            info.size();
        GLsizei real_stride =
            info.stride() ? info.stride() : bytes_per_element;
        GLsizei bytes_collected = CollectData(
            info.pointer(), bytes_per_element, real_stride, num_elements);
        gl->BufferSubData(
            GL_ARRAY_BUFFER, array_buffer_offset_, bytes_collected,
            collection_buffer_.get());
        gl_helper->VertexAttribPointer(
            ii, info.size(), info.type(), info.normalized(), 0,
            array_buffer_offset_);
        array_buffer_offset_ += RoundUpToMultipleOf4(bytes_collected);
        DCHECK_LE(array_buffer_offset_, array_buffer_size_);
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
      gl->BufferData(GL_ELEMENT_ARRAY_BUFFER, bytes_needed, NULL,
                     GL_DYNAMIC_DRAW);
    }
    gl->BufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, bytes_needed, indices);
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

#endif  // defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)


#if !defined(COMPILER_MSVC)
const size_t GLES2Implementation::kMaxSizeOfSimpleResult;
#endif

COMPILE_ASSERT(gpu::kInvalidResource == 0,
               INVALID_RESOURCE_NOT_0_AS_GL_EXPECTS);

GLES2Implementation::GLES2Implementation(
      GLES2CmdHelper* helper,
      size_t transfer_buffer_size,
      void* transfer_buffer,
      int32 transfer_buffer_id)
    : util_(0),  // TODO(gman): Get real number of compressed texture formats.
      helper_(helper),
      transfer_buffer_(transfer_buffer_size, helper, transfer_buffer),
      transfer_buffer_id_(transfer_buffer_id),
      pack_alignment_(4),
      unpack_alignment_(4),
#if defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)
      bound_array_buffer_id_(0),
      bound_element_array_buffer_id_(0),
#endif
      error_bits_(0) {
  // Allocate space for simple GL results.
  result_buffer_ = transfer_buffer_.Alloc(kMaxSizeOfSimpleResult);
  result_shm_offset_ = transfer_buffer_.GetOffset(result_buffer_);

#if defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)
  GLint max_vertex_attribs;
  GetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attribs);
  id_allocator_.MarkAsUsed(kClientSideArrayId);
  id_allocator_.MarkAsUsed(kClientSideElementArrayId);

  reserved_ids_[0] = kClientSideArrayId;
  reserved_ids_[1] = kClientSideElementArrayId;

  client_side_buffer_helper_.reset(new ClientSideBufferHelper(
      max_vertex_attribs,
      kClientSideArrayId,
      kClientSideElementArrayId));
#endif
}

GLES2Implementation::~GLES2Implementation() {
  GLuint buffers[] = { kClientSideArrayId, kClientSideElementArrayId, };
  DeleteBuffers(arraysize(buffers), &buffers[0]);
  transfer_buffer_.Free(result_buffer_);
}

void GLES2Implementation::MakeIds(GLsizei n, GLuint* ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    ids[ii] = id_allocator_.AllocateID();
  }
}

void GLES2Implementation::FreeIds(GLsizei n, const GLuint* ids) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    id_allocator_.FreeID(ids[ii]);
  }
}

void GLES2Implementation::WaitForCmd() {
  helper_->CommandBufferHelper::Finish();
}

GLenum GLES2Implementation::GetError() {
  return GetGLError();
}

GLenum GLES2Implementation::GetGLError() {
  // Check the GL error first, then our wrapped error.
  typedef gles2::GetError::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = GL_NO_ERROR;
  helper_->GetError(result_shm_id(), result_shm_offset());
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

void GLES2Implementation::SetGLError(GLenum error) {
  error_bits_ |= GLES2Util::GLErrorToErrorBit(error);
}

void GLES2Implementation::GetBucketContents(uint32 bucket_id,
                                            std::vector<int8>* data) {
  DCHECK(data);
  typedef cmd::GetBucketSize::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->GetBucketSize(bucket_id, result_shm_id(), result_shm_offset());
  WaitForCmd();
  uint32 size = *result;
  data->resize(size);
  if (size > 0u) {
    uint32 max_size = transfer_buffer_.GetLargestFreeOrPendingSize();
    uint32 offset = 0;
    while (size) {
      uint32 part_size = std::min(max_size, size);
      void* buffer = transfer_buffer_.Alloc(part_size);
      helper_->GetBucketData(
          bucket_id, offset, part_size,
          transfer_buffer_id_, transfer_buffer_.GetOffset(buffer));
      WaitForCmd();
      memcpy(&(*data)[offset], buffer, part_size);
      transfer_buffer_.Free(buffer);
      offset += part_size;
      size -= part_size;
    }
    // Free the bucket. This is not required but it does free up the memory.
    // and we don't have to wait for the result so from the client's perspective
    // it's cheap.
    helper_->SetBucketSize(bucket_id, 0);
  }
}

void GLES2Implementation::SetBucketContents(
    uint32 bucket_id, const void* data, size_t size) {
  DCHECK(data);
  helper_->SetBucketSize(bucket_id, size);
  if (size > 0u) {
    uint32 max_size = transfer_buffer_.GetLargestFreeOrPendingSize();
    uint32 offset = 0;
    while (size) {
      uint32 part_size = std::min(static_cast<size_t>(max_size), size);
      void* buffer = transfer_buffer_.Alloc(part_size);
      memcpy(buffer, static_cast<const int8*>(data) + offset, part_size);
      helper_->SetBucketData(
          bucket_id, offset, part_size,
          transfer_buffer_id_, transfer_buffer_.GetOffset(buffer));
      transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
      offset += part_size;
      size -= part_size;
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
  DCHECK(str);
  std::vector<int8> data;
  // NOTE: strings are passed NULL terminated. That means the empty
  // string will have a size of 1 and no-string will have a size of 0
  GetBucketContents(bucket_id, &data);
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

void GLES2Implementation::DrawElements(
    GLenum mode, GLsizei count, GLenum type, const void* indices) {
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  if (count == 0) {
    return;
  }
#if defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)
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
      num_elements = GetMaxValueInBuffer(
          bound_element_array_buffer_id_, count, type, ToGLuint(indices)) + 1;
    }
  }
  if (have_client_side) {
    client_side_buffer_helper_->SetupSimualtedClientSideBuffers(
        this, helper_, num_elements);
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
  // Insert the cmd to call glFlush
  helper_->Flush();
  // Flush our command buffer
  // (tell the service to execute upto the flush cmd.)
  helper_->CommandBufferHelper::Flush();
}

void GLES2Implementation::Finish() {
  // Insert the cmd to call glFinish
  helper_->Finish();
  // Finish our command buffer
  // (tell the service to execute upto the Finish cmd and wait for it to
  // execute.)
  helper_->CommandBufferHelper::Finish();
}

void GLES2Implementation::SwapBuffers() {
  helper_->SwapBuffers();
  Flush();
}

void GLES2Implementation::BindAttribLocation(
  GLuint program, GLuint index, const char* name) {
  SetBucketAsString(kResultBucketId, name);
  helper_->BindAttribLocationBucket(program, index, kResultBucketId);
  helper_->SetBucketSize(kResultBucketId, 0);
}

void GLES2Implementation::GetVertexAttribPointerv(
    GLuint index, GLenum pname, void** ptr) {
#if defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)
  // If it's a client side buffer the client has the data.
  if (client_side_buffer_helper_->GetAttribPointer(index, pname, ptr)) {
    return;
  }
#endif  // defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)

  typedef gles2::GetVertexAttribPointerv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetVertexAttribPointerv(
    index, pname, result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(ptr);
};

GLint GLES2Implementation::GetAttribLocation(
    GLuint program, const char* name) {
  typedef GetAttribLocationBucket::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = -1;
  SetBucketAsCString(kResultBucketId, name);
  helper_->GetAttribLocationBucket(program, kResultBucketId,
                                   result_shm_id(), result_shm_offset());
  WaitForCmd();
  helper_->SetBucketSize(kResultBucketId, 0);
  return *result;
}

GLint GLES2Implementation::GetUniformLocation(
    GLuint program, const char* name) {
  typedef GetUniformLocationBucket::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = -1;
  SetBucketAsCString(kResultBucketId, name);
  helper_->GetUniformLocationBucket(program, kResultBucketId,
                                    result_shm_id(), result_shm_offset());
  WaitForCmd();
  helper_->SetBucketSize(kResultBucketId, 0);
  return *result;
}


void GLES2Implementation::ShaderBinary(
    GLsizei n, const GLuint* shaders, GLenum binaryformat, const void* binary,
    GLsizei length) {
  if (n < 0 || length < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  GLsizei shader_id_size = n * sizeof(*shaders);
  void* shader_ids = transfer_buffer_.Alloc(shader_id_size);
  void* shader_data = transfer_buffer_.Alloc(length);
  memcpy(shader_ids, shaders, shader_id_size);
  memcpy(shader_data, binary, length);
  helper_->ShaderBinary(
      n,
      transfer_buffer_id_, transfer_buffer_.GetOffset(shader_ids),
      binaryformat,
      transfer_buffer_id_, transfer_buffer_.GetOffset(shader_data),
      length);
  int32 token = helper_->InsertToken();
  transfer_buffer_.FreePendingToken(shader_ids, token);
  transfer_buffer_.FreePendingToken(shader_data, token);
}

void GLES2Implementation::PixelStorei(GLenum pname, GLint param) {
  switch (pname) {
  case GL_PACK_ALIGNMENT:
      pack_alignment_ = param;
      break;
  case GL_UNPACK_ALIGNMENT:
      unpack_alignment_ = param;
      break;
  default:
      break;
  }
  helper_->PixelStorei(pname, param);
}


void GLES2Implementation::VertexAttribPointer(
    GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
    const void* ptr) {
#if defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)
  // Record the info on the client side.
  client_side_buffer_helper_->SetAttribPointer(
      bound_array_buffer_id_, index, size, type, normalized, stride, ptr);
  if (bound_array_buffer_id_ != 0) {
    // Only report NON client side buffers to the service.
    helper_->VertexAttribPointer(index, size, type, normalized, stride,
                                 ToGLuint(ptr));
  }
#else  // !defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)
  helper_->VertexAttribPointer(index, size, type, normalized, stride,
                               ToGLuint(ptr));
#endif  // !defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)
}

void GLES2Implementation::ShaderSource(
    GLuint shader, GLsizei count, const char** source, const GLint* length) {
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }

  // Compute the total size.
  uint32 total_size = 1;
  for (GLsizei ii = 0; ii < count; ++ii) {
    total_size += length ? length[ii] : strlen(source[ii]);
  }

  // Concatenate all the strings in to a bucket on the service.
  helper_->SetBucketSize(kResultBucketId, total_size);
  uint32 max_size = transfer_buffer_.GetLargestFreeOrPendingSize();
  uint32 offset = 0;
  for (GLsizei ii = 0; ii <= count; ++ii) {
    const char* src = ii < count ? source[ii] : "";
    uint32 size = ii < count ? (length ? length[ii] : strlen(src)) : 1;
    while (size) {
      uint32 part_size = std::min(size, max_size);
      void* buffer = transfer_buffer_.Alloc(part_size);
      memcpy(buffer, src, part_size);
      helper_->SetBucketData(kResultBucketId, offset, part_size,
                             transfer_buffer_id_,
                             transfer_buffer_.GetOffset(buffer));
      transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
      offset += part_size;
      src += part_size;
      size -= part_size;
    }
  }

  DCHECK_EQ(total_size, offset);

  helper_->ShaderSourceBucket(shader, kResultBucketId);
  helper_->SetBucketSize(kResultBucketId, 0);
}

void GLES2Implementation::BufferData(
    GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
  // NOTE: Should this be optimized for the case where we can call BufferData
  //    with the actual data in the case of our transfer buffer being big
  //    enough?
  helper_->BufferData(target, size, 0, 0, usage);
  if (data != NULL) {
    BufferSubData(target, 0, size, data);
  }
}

void GLES2Implementation::BufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
  if (size == 0) {
    return;
  }

  if (size < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }

  const int8* source = static_cast<const int8*>(data);
  GLsizeiptr max_size = transfer_buffer_.GetLargestFreeOrPendingSize();
  while (size) {
    GLsizeiptr part_size = std::min(size, max_size);
    void* buffer = transfer_buffer_.Alloc(part_size);
    memcpy(buffer, source, part_size);
    helper_->BufferSubData(target, offset, part_size,
                           transfer_buffer_id_,
                           transfer_buffer_.GetOffset(buffer));
    transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
    offset += part_size;
    source += part_size;
    size -= part_size;
  }
}

void GLES2Implementation::CompressedTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLsizei width,
    GLsizei height, GLint border, GLsizei image_size, const void* data) {
  if (width < 0 || height < 0 || level < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  // TODO(gman): Switch to use buckets always or at least if no room in shared
  //    memory.
  DCHECK_LE(image_size,
            static_cast<GLsizei>(
                transfer_buffer_.GetLargestFreeOrPendingSize()));
  void* buffer = transfer_buffer_.Alloc(image_size);
  memcpy(buffer, data, image_size);
  helper_->CompressedTexImage2D(
      target, level, internalformat, width, height, border, image_size,
      transfer_buffer_id_, transfer_buffer_.GetOffset(buffer));
  transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
}

void GLES2Implementation::CompressedTexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLsizei image_size, const void* data) {
  if (width < 0 || height < 0 || level < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  // TODO(gman): Switch to use buckets always or at least if no room in shared
  //    memory.
  DCHECK_LE(image_size,
            static_cast<GLsizei>(
                transfer_buffer_.GetLargestFreeOrPendingSize()));
  void* buffer = transfer_buffer_.Alloc(image_size);
  memcpy(buffer, data, image_size);
  helper_->CompressedTexSubImage2D(
      target, level, xoffset, yoffset, width, height, format, image_size,
      transfer_buffer_id_, transfer_buffer_.GetOffset(buffer));
  transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
}

void GLES2Implementation::TexImage2D(
    GLenum target, GLint level, GLint internalformat, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  if (level < 0 || height < 0 || width < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  uint32 size;
  if (!GLES2Util::ComputeImageDataSize(
      width, height, format, type, unpack_alignment_, &size)) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  helper_->TexImage2D(
      target, level, internalformat, width, height, border, format, type, 0, 0);
  if (pixels) {
    TexSubImage2D(target, level, 0, 0, width, height, format, type, pixels);
  }
}

void GLES2Implementation::TexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void* pixels) {
  if (level < 0 || height < 0 || width < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  const int8* source = static_cast<const int8*>(pixels);
  GLsizeiptr max_size = transfer_buffer_.GetLargestFreeOrPendingSize();
  uint32 temp_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, 1, format, type, unpack_alignment_, &temp_size)) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  GLsizeiptr unpadded_row_size = temp_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, 2, format, type, unpack_alignment_, &temp_size)) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  GLsizeiptr padded_row_size = temp_size - unpadded_row_size;
  if (padded_row_size < 0 || unpadded_row_size < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }

  if (padded_row_size <= max_size) {
    // Transfer by rows.
    GLint max_rows = max_size / padded_row_size;
    while (height) {
      GLint num_rows = std::min(height, max_rows);
      GLsizeiptr part_size = num_rows * padded_row_size;
      void* buffer = transfer_buffer_.Alloc(part_size);
      memcpy(buffer, source, part_size);
      helper_->TexSubImage2D(
          target, level, xoffset, yoffset, width, num_rows, format, type,
          transfer_buffer_id_, transfer_buffer_.GetOffset(buffer));
      transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
      yoffset += num_rows;
      source += part_size;
      height -= num_rows;
    }
  } else {
    // Transfer by sub rows. Beacuse GL has no maximum texture dimensions.
    uint32 temp;
    GLES2Util::ComputeImageDataSize(
       1, 1, format, type, unpack_alignment_, &temp);
    GLsizeiptr element_size = temp;
    max_size -= max_size % element_size;
    GLint max_sub_row_pixels = max_size / element_size;
    for (; height; --height) {
      GLint temp_width = width;
      GLint temp_xoffset = xoffset;
      const int8* row_source = source;
      while (temp_width) {
        GLint num_pixels = std::min(width, max_sub_row_pixels);
        GLsizeiptr part_size = num_pixels * element_size;
        void* buffer = transfer_buffer_.Alloc(part_size);
        memcpy(buffer, row_source, part_size);
        helper_->TexSubImage2D(
            target, level, temp_xoffset, yoffset, temp_width, 1, format, type,
            transfer_buffer_id_, transfer_buffer_.GetOffset(buffer));
        transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
        row_source += part_size;
        temp_xoffset += num_pixels;
        temp_width -= num_pixels;
      }
      ++yoffset;
      source += padded_row_size;
    }
  }
}

void GLES2Implementation::GetActiveAttrib(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  // Clear the bucket so if we the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  typedef gles2::GetActiveAttrib::Result Result;
  Result* result = static_cast<Result*>(result_buffer_);
  // Set as failed so if the command fails we'll recover.
  result->success = false;
  helper_->GetActiveAttrib(program, index, kResultBucketId,
                           result_shm_id(), result_shm_offset());
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
                                  str.size());
      if (length) {
        *length = max_size;
      }
      if (name && bufsize > 0) {
        memcpy(name, &str[0], max_size);
        name[max_size] = '\0';
      }
    }
  }
}

void GLES2Implementation::GetActiveUniform(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  // Clear the bucket so if we the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  typedef gles2::GetActiveUniform::Result Result;
  Result* result = static_cast<Result*>(result_buffer_);
  // Set as failed so if the command fails we'll recover.
  result->success = false;
  helper_->GetActiveUniform(program, index, kResultBucketId,
                            result_shm_id(), result_shm_offset());
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
                                  str.size());
      if (length) {
        *length = max_size;
      }
      if (name && bufsize > 0) {
        memcpy(name, &str[0], max_size);
        name[max_size] = '\0';
      }
    }
  }
}

void GLES2Implementation::GetAttachedShaders(
    GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders) {
  if (maxcount < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  typedef gles2::GetAttachedShaders::Result Result;
  uint32 size = Result::ComputeSize(maxcount);
  Result* result = transfer_buffer_.AllocTyped<Result>(size);
  helper_->GetAttachedShaders(
    program,
    transfer_buffer_id_,
    transfer_buffer_.GetOffset(result),
    size);
  int32 token = helper_->InsertToken();
  WaitForCmd();
  if (count) {
    *count = result->GetNumResults();
  }
  result->CopyResult(shaders);
  transfer_buffer_.FreePendingToken(result, token);
}

void GLES2Implementation::GetShaderPrecisionFormat(
    GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
  typedef gles2::GetShaderPrecisionFormat::Result Result;
  Result* result = static_cast<Result*>(result_buffer_);
  helper_->GetShaderPrecisionFormat(
    shadertype, precisiontype, result_shm_id(), result_shm_offset());
  WaitForCmd();
  if (result->success) {
    if (range) {
      range[0] = result->min_range;
      range[1] = result->max_range;
    }
    if (precision) {
      precision[0] = result->precision;
    }
  }
}

const GLubyte* GLES2Implementation::GetString(GLenum name) {
  const char* result;
  GLStringMap::const_iterator it = gl_strings_.find(name);
  if (it != gl_strings_.end()) {
    result = it->second.c_str();
  } else {
    // Clear the bucket so if we the command fails nothing will be in it.
    helper_->SetBucketSize(kResultBucketId, 0);
    helper_->GetString(name, kResultBucketId);
    std::string str;
    if (GetBucketAsString(kResultBucketId, &str)) {
      std::pair<GLStringMap::const_iterator, bool> insert_result =
          gl_strings_.insert(std::make_pair(name, str));
      DCHECK(insert_result.second);
      result = insert_result.first->second.c_str();
    } else {
      result = NULL;
    }
  }
  return reinterpret_cast<const GLubyte*>(result);
}

void GLES2Implementation::GetUniformfv(
    GLuint program, GLint location, GLfloat* params) {
  helper_->GetUniformfv(
      program, location, result_shm_id(), result_shm_offset());
  WaitForCmd();
  static_cast<gles2::GetUniformfv::Result*>(result_buffer_)->CopyResult(params);
}

void GLES2Implementation::GetUniformiv(
    GLuint program, GLint location, GLint* params) {
  helper_->GetUniformiv(
      program, location, result_shm_id(), result_shm_offset());
  WaitForCmd();
  static_cast<gles2::GetUniformfv::Result*>(result_buffer_)->CopyResult(params);
}

void GLES2Implementation::ReadPixels(
    GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format,
    GLenum type, void* pixels) {
  if (width < 0 || height < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  if (width == 0 || height == 0) {
    return;
  }

  // glReadPixel pads the size of each row of pixels by an ammount specified by
  // glPixelStorei. So, we have to take that into account both in the fact that
  // the pixels returned from the ReadPixel command will include that padding
  // and that when we copy the results to the user's buffer we need to not
  // write those padding bytes but leave them as they are.

  typedef gles2::ReadPixels::Result Result;
  Result* result = static_cast<Result*>(result_buffer_);
  int8* dest = reinterpret_cast<int8*>(pixels);
  GLsizeiptr max_size = transfer_buffer_.GetLargestFreeOrPendingSize();
  uint32 temp_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, 1, format, type, pack_alignment_, &temp_size)) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  GLsizeiptr unpadded_row_size = temp_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, 2, format, type, pack_alignment_, &temp_size)) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  GLsizeiptr padded_row_size = temp_size - unpadded_row_size;
  if (padded_row_size < 0 || unpadded_row_size < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  // Check if we have enough space to transfer at least an entire row.
  if (padded_row_size <= max_size) {
    // Transfer by rows.
    // The max rows we can transfer.
    GLint max_rows = max_size / padded_row_size;
    while (height) {
      // Compute how many rows to transfer.
      GLint num_rows = std::min(height, max_rows);
      // Compute how much space those rows will take. The last row will not
      // include padding.
      GLsizeiptr part_size =
          unpadded_row_size + (padded_row_size * std::max(num_rows - 1, 0));
      void* buffer = transfer_buffer_.Alloc(part_size);
      *result = 0;  // mark as failed.
      helper_->ReadPixels(
          xoffset, yoffset, width, num_rows, format, type,
          transfer_buffer_id_, transfer_buffer_.GetOffset(buffer),
          result_shm_id(), result_shm_offset());
      WaitForCmd();
      // If it was not marked as successful exit.
      if (*result == 0) {
        return;
      }
      // We have to copy 1 row at a time to avoid writing pad bytes.
      const int8* src = static_cast<const int8*>(buffer);
      for (GLint yy = 0; yy < num_rows; ++yy) {
        memcpy(dest, src, unpadded_row_size);
        dest += padded_row_size;
        src += padded_row_size;
      }
      transfer_buffer_.Free(buffer);
      yoffset += num_rows;
      height -= num_rows;
    }
  } else {
    // Transfer by sub rows. Beacuse GL has no maximum texture dimensions.
    GLES2Util::ComputeImageDataSize(
       1, 1, format, type, pack_alignment_, &temp_size);
    GLsizeiptr element_size = temp_size;
    max_size -= max_size % element_size;
    GLint max_sub_row_pixels = max_size / element_size;
    for (; height; --height) {
      GLint temp_width = width;
      GLint temp_xoffset = xoffset;
      int8* row_dest = dest;
      while (temp_width) {
        GLint num_pixels = std::min(width, max_sub_row_pixels);
        GLsizeiptr part_size = num_pixels * element_size;
        void* buffer = transfer_buffer_.Alloc(part_size);
        *result = 0;  // mark as failed.
        helper_->ReadPixels(
            temp_xoffset, yoffset, temp_width, 1, format, type,
            transfer_buffer_id_, transfer_buffer_.GetOffset(buffer),
            result_shm_id(), result_shm_offset());
        WaitForCmd();
        // If it was not marked as successful exit.
        if (*result == 0) {
          return;
        }
        memcpy(row_dest, buffer, part_size);
        transfer_buffer_.Free(buffer);
        row_dest += part_size;
        temp_xoffset += num_pixels;
        temp_width -= num_pixels;
      }
      ++yoffset;
      dest += padded_row_size;
    }
  }
}

#if defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)
bool GLES2Implementation::IsReservedId(GLuint id) {
  for (size_t ii = 0; ii < arraysize(reserved_ids_); ++ii) {
    if (id == reserved_ids_[ii]) {
      return true;
    }
  }
  return false;
}
#else
bool GLES2Implementation::IsReservedId(GLuint) {  // NOLINT
  return false;
}
#endif

#if defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)

void GLES2Implementation::BindBuffer(GLenum target, GLuint buffer) {
  if (IsReservedId(buffer)) {
    SetGLError(GL_INVALID_OPERATION);
    return;
  }
  if (buffer != 0) {
    id_allocator_.MarkAsUsed(buffer);
  }
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
  helper_->BindBuffer(target, buffer);
}

void GLES2Implementation::DeleteBuffers(GLsizei n, const GLuint* buffers) {
  FreeIds(n, buffers);
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (buffers[ii] == bound_array_buffer_id_) {
      bound_array_buffer_id_ = 0;
    }
    if (buffers[ii] == bound_element_array_buffer_id_) {
      bound_element_array_buffer_id_ = 0;
    }
  }
  // TODO(gman): compute the number of buffers we can delete in 1 call
  //    based on the size of command buffer and the limit of argument size
  //    for comments then loop to delete all the buffers.  The same needs to
  //    happen for GenBuffer, GenTextures, DeleteTextures, etc...
  helper_->DeleteBuffersImmediate(n, buffers);
}

void GLES2Implementation::DisableVertexAttribArray(GLuint index) {
  client_side_buffer_helper_->SetAttribEnable(index, false);
  helper_->DisableVertexAttribArray(index);
}

void GLES2Implementation::EnableVertexAttribArray(GLuint index) {
  client_side_buffer_helper_->SetAttribEnable(index, true);
  helper_->EnableVertexAttribArray(index);
}

void GLES2Implementation::DrawArrays(GLenum mode, GLint first, GLsizei count) {
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE);
    return;
  }
  bool have_client_side =
      client_side_buffer_helper_->HaveEnabledClientSideBuffers();
  if (have_client_side) {
    client_side_buffer_helper_->SetupSimualtedClientSideBuffers(
        this, helper_, first + count);
  }
  helper_->DrawArrays(mode, first, count);
  if (have_client_side) {
    // Restore the user's current binding.
    helper_->BindBuffer(GL_ARRAY_BUFFER, bound_array_buffer_id_);
  }
}

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
      SetGLError(GL_INVALID_ENUM);
      break;
  }
  return true;
}

void GLES2Implementation::GetVertexAttribfv(
    GLuint index, GLenum pname, GLfloat* params) {
  uint32 value;
  if (GetVertexAttribHelper(index, pname, &value)) {
    *params = static_cast<float>(value);
    return;
  }
  typedef GetVertexAttribfv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetVertexAttribfv(
      index, pname, result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
}

void GLES2Implementation::GetVertexAttribiv(
    GLuint index, GLenum pname, GLint* params) {
  uint32 value;
  if (GetVertexAttribHelper(index, pname, &value)) {
    *params = value;
    return;
  }
  typedef GetVertexAttribiv::Result Result;
  Result* result = GetResultAs<Result*>();
  result->SetNumResults(0);
  helper_->GetVertexAttribiv(
      index, pname, result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
}

#endif  // defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)

}  // namespace gles2
}  // namespace gpu
