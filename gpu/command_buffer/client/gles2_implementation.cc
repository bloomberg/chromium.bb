// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class to emluate GLES2 over command buffers.

#include "../client/gles2_implementation.h"
#include <GLES2/gles2_command_buffer.h>
#include "../client/mapped_memory.h"
#include "../common/gles2_cmd_utils.h"
#include "../common/id_allocator.h"

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

// An id handler for non-shared ids.
class NonSharedIdHandler : public IdHandlerInterface {
 public:
  NonSharedIdHandler() { }
  virtual ~NonSharedIdHandler() { }

  // Overridden from IdHandlerInterface.
  virtual void MakeIds(GLuint id_offset, GLsizei n, GLuint* ids) {
    if (id_offset == 0) {
      for (GLsizei ii = 0; ii < n; ++ii) {
        ids[ii] = id_allocator_.AllocateID();
      }
    } else {
      for (GLsizei ii = 0; ii < n; ++ii) {
        ids[ii] = id_allocator_.AllocateIDAtOrAbove(id_offset);
        id_offset = ids[ii] + 1;
      }
    }
  }

  // Overridden from IdHandlerInterface.
  virtual void FreeIds(GLsizei n, const GLuint* ids) {
    for (GLsizei ii = 0; ii < n; ++ii) {
      id_allocator_.FreeID(ids[ii]);
    }
  }

  // Overridden from IdHandlerInterface.
  virtual bool MarkAsUsedForBind(GLuint id) {
    return id == 0 ? true : id_allocator_.MarkAsUsed(id);
  }
 private:
  IdAllocator id_allocator_;
};

// An id handler for non-shared ids that are never reused.
class NonSharedNonReusedIdHandler : public IdHandlerInterface {
 public:
  NonSharedNonReusedIdHandler() : last_id_(0) { }
  virtual ~NonSharedNonReusedIdHandler() { }

  // Overridden from IdHandlerInterface.
  virtual void MakeIds(GLuint id_offset, GLsizei n, GLuint* ids) {
    for (GLsizei ii = 0; ii < n; ++ii) {
      ids[ii] = ++last_id_ + id_offset;
    }
  }

  // Overridden from IdHandlerInterface.
  virtual void FreeIds(GLsizei /* n */, const GLuint* /* ids */) {
    // Ids are never freed.
  }

  // Overridden from IdHandlerInterface.
  virtual bool MarkAsUsedForBind(GLuint /* id */) {
    // This is only used for Shaders and Programs which have no bind.
    return false;
  }

 private:
  GLuint last_id_;
};

// An id handler for shared ids.
class SharedIdHandler : public IdHandlerInterface {
 public:
  SharedIdHandler(
    GLES2Implementation* gles2,
    id_namespaces::IdNamespaces id_namespace)
      : gles2_(gles2),
        id_namespace_(id_namespace) {
  }

  virtual ~SharedIdHandler() { }

  virtual void MakeIds(GLuint id_offset, GLsizei n, GLuint* ids) {
    gles2_->GenSharedIdsCHROMIUM(id_namespace_, id_offset, n, ids);
  }

  virtual void FreeIds(GLsizei n, const GLuint* ids) {
    gles2_->DeleteSharedIdsCHROMIUM(id_namespace_, n, ids);
  }

  virtual bool MarkAsUsedForBind(GLuint) {  // NOLINT
    // This has no meaning for shared resources.
    return true;
  }

 private:
  GLES2Implementation* gles2_;
  id_namespaces::IdNamespaces id_namespace_;
};

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
        GLsizei real_stride = info.stride() ?
            info.stride() : static_cast<GLsizei>(bytes_per_element);
        GLsizei bytes_collected = CollectData(
            info.pointer(), bytes_per_element, real_stride, num_elements);
        gl->BufferSubData(
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


#if !defined(_MSC_VER)
const size_t GLES2Implementation::kMaxSizeOfSimpleResult;
#endif

COMPILE_ASSERT(gpu::kInvalidResource == 0,
               INVALID_RESOURCE_NOT_0_AS_GL_EXPECTS);

GLES2Implementation::GLES2Implementation(
      GLES2CmdHelper* helper,
      size_t transfer_buffer_size,
      void* transfer_buffer,
      int32 transfer_buffer_id,
      bool share_resources)
    : util_(0),  // TODO(gman): Get real number of compressed texture formats.
      helper_(helper),
      transfer_buffer_(
          kStartingOffset,
          transfer_buffer_size - kStartingOffset,
          helper,
          static_cast<char*>(transfer_buffer) + kStartingOffset),
      transfer_buffer_id_(transfer_buffer_id),
      pack_alignment_(4),
      unpack_alignment_(4),
#if defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)
      bound_array_buffer_id_(0),
      bound_element_array_buffer_id_(0),
      client_side_array_id_(0),
      client_side_element_array_id_(0),
#endif
      error_bits_(0) {
  // Allocate space for simple GL results.
  result_buffer_ = transfer_buffer;
  result_shm_offset_ = 0;

  mapped_memory_.reset(new MappedMemoryManager(helper_));

  if (share_resources) {
    buffer_id_handler_.reset(
        new SharedIdHandler(this, id_namespaces::kBuffers));
    framebuffer_id_handler_.reset(
        new SharedIdHandler(this, id_namespaces::kFramebuffers));
    renderbuffer_id_handler_.reset(
        new SharedIdHandler(this, id_namespaces::kRenderbuffers));
    program_and_shader_id_handler_.reset(
        new SharedIdHandler(this, id_namespaces::kProgramsAndShaders));
    texture_id_handler_.reset(
        new SharedIdHandler(this, id_namespaces::kTextures));
  } else {
    buffer_id_handler_.reset(new NonSharedIdHandler());
    framebuffer_id_handler_.reset(new NonSharedIdHandler());
    renderbuffer_id_handler_.reset(new NonSharedIdHandler());
    program_and_shader_id_handler_.reset(new NonSharedNonReusedIdHandler());
    texture_id_handler_.reset(new NonSharedIdHandler());
  }

#if defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)
  GLint max_vertex_attribs;
  GetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attribs);

  buffer_id_handler_->MakeIds(
      kClientSideArrayId, arraysize(reserved_ids_), &reserved_ids_[0]);

  client_side_buffer_helper_.reset(new ClientSideBufferHelper(
      max_vertex_attribs,
      reserved_ids_[0],
      reserved_ids_[1]));
#endif
}

GLES2Implementation::~GLES2Implementation() {
  DeleteBuffers(arraysize(reserved_ids_), &reserved_ids_[0]);
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

void GLES2Implementation::SetGLError(GLenum error, const char* msg) {
  if (msg) {
    last_error_ = msg;
  }
  error_bits_ |= GLES2Util::GLErrorToErrorBit(error);
}

void GLES2Implementation::GetBucketContents(uint32 bucket_id,
                                            std::vector<int8>* data) {
  GPU_DCHECK(data);
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
      transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
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
  GPU_DCHECK(data);
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
  GPU_DCHECK(str);
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
    SetGLError(GL_INVALID_VALUE, "glDrawElements: count less than 0.");
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
      num_elements = GetMaxValueInBufferCHROMIUM(
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
  // Wait if this would add too many swap buffers.
  if (swap_buffers_tokens_.size() == kMaxSwapBuffers) {
    helper_->WaitForToken(swap_buffers_tokens_.front());
    swap_buffers_tokens_.pop();
  }
  helper_->SwapBuffers();
  swap_buffers_tokens_.push(helper_->InsertToken());
  Flush();
}

void GLES2Implementation::CopyTextureToParentTextureCHROMIUM(
    GLuint client_child_id, GLuint client_parent_id) {
  // Wait if this would add too many CopyTextureToParentTexture's
  if (swap_buffers_tokens_.size() == kMaxSwapBuffers) {
    helper_->WaitForToken(swap_buffers_tokens_.front());
    swap_buffers_tokens_.pop();
  }
  helper_->CopyTextureToParentTextureCHROMIUM(client_child_id,
      client_parent_id);
  swap_buffers_tokens_.push(helper_->InsertToken());
  Flush();
}

void GLES2Implementation::GenSharedIdsCHROMIUM(
  GLuint namespace_id, GLuint id_offset, GLsizei n, GLuint* ids) {
  GLint* id_buffer = transfer_buffer_.AllocTyped<GLint>(n);
  helper_->GenSharedIdsCHROMIUM(namespace_id, id_offset, n,
                        transfer_buffer_id_,
                        transfer_buffer_.GetOffset(id_buffer));
  WaitForCmd();
  memcpy(ids, id_buffer, sizeof(*ids) * n);
  transfer_buffer_.FreePendingToken(id_buffer, helper_->InsertToken());
}

void GLES2Implementation::DeleteSharedIdsCHROMIUM(
    GLuint namespace_id, GLsizei n, const GLuint* ids) {
  GLint* id_buffer = transfer_buffer_.AllocTyped<GLint>(n);
  memcpy(id_buffer, ids, sizeof(*ids) * n);
  helper_->DeleteSharedIdsCHROMIUM(namespace_id, n,
                           transfer_buffer_id_,
                           transfer_buffer_.GetOffset(id_buffer));
  WaitForCmd();
  transfer_buffer_.FreePendingToken(id_buffer, helper_->InsertToken());
}

void GLES2Implementation::RegisterSharedIdsCHROMIUM(
    GLuint namespace_id, GLsizei n, const GLuint* ids) {
  GLint* id_buffer = transfer_buffer_.AllocTyped<GLint>(n);
  memcpy(id_buffer, ids, sizeof(*ids) * n);
  helper_->RegisterSharedIdsCHROMIUM(namespace_id, n,
                             transfer_buffer_id_,
                             transfer_buffer_.GetOffset(id_buffer));
  WaitForCmd();
  transfer_buffer_.FreePendingToken(id_buffer, helper_->InsertToken());
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
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glShaderBinary n < 0.");
    return;
  }
  if (length < 0) {
    SetGLError(GL_INVALID_VALUE, "glShaderBinary length < 0.");
    return;
  }
  GLsizei shader_id_size = n * sizeof(*shaders);
  int8* buffer = transfer_buffer_.AllocTyped<int8>(shader_id_size + length);
  void* shader_ids = buffer;
  void* shader_data = buffer + shader_id_size;
  memcpy(shader_ids, shaders, shader_id_size);
  memcpy(shader_data, binary, length);
  helper_->ShaderBinary(
      n,
      transfer_buffer_id_, transfer_buffer_.GetOffset(shader_ids),
      binaryformat,
      transfer_buffer_id_, transfer_buffer_.GetOffset(shader_data),
      length);
  transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
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
  uint32 max_size = transfer_buffer_.GetLargestFreeOrPendingSize();
  uint32 offset = 0;
  for (GLsizei ii = 0; ii <= count; ++ii) {
    const char* src = ii < count ? source[ii] : "";
    if (src) {
      uint32 size = ii < count ?
          (length ? static_cast<size_t>(length[ii]) : strlen(src)) : 1;
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
  }

  GPU_DCHECK_EQ(total_size, offset);

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
    SetGLError(GL_INVALID_VALUE, "glBufferSubData: size < 0");
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

void GLES2Implementation::TexImage2D(
    GLenum target, GLint level, GLint internalformat, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
  if (level < 0 || height < 0 || width < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexImage2D dimension < 0");
    return;
  }
  uint32 size;
  if (!GLES2Util::ComputeImageDataSize(
      width, height, format, type, unpack_alignment_, &size)) {
    SetGLError(GL_INVALID_VALUE, "glTexImage2D: image size too large");
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
    SetGLError(GL_INVALID_VALUE, "glTexSubImage2D dimension < 0");
    return;
  }
  if (height == 0 || width == 0) {
    return;
  }
  const int8* source = static_cast<const int8*>(pixels);
  GLsizeiptr max_size = transfer_buffer_.GetLargestFreeOrPendingSize();
  uint32 temp_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, 1, format, type, unpack_alignment_, &temp_size)) {
    SetGLError(GL_INVALID_VALUE, "glTexSubImage2D: size to large");
    return;
  }
  GLsizeiptr unpadded_row_size = temp_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, 2, format, type, unpack_alignment_, &temp_size)) {
    SetGLError(GL_INVALID_VALUE, "glTexSubImage2D: size to large");
    return;
  }
  GLsizeiptr padded_row_size = temp_size - unpadded_row_size;
  if (padded_row_size < 0 || unpadded_row_size < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexSubImage2D: size to large");
    return;
  }

  if (padded_row_size <= max_size) {
    // Transfer by rows.
    GLint max_rows = max_size / std::max(padded_row_size,
                                         static_cast<GLsizeiptr>(1));
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
    SetGLError(GL_INVALID_VALUE, "glGetActiveAttrib: bufsize < 0");
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
}

void GLES2Implementation::GetActiveUniform(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetActiveUniform: bufsize < 0");
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
}

void GLES2Implementation::GetAttachedShaders(
    GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders) {
  if (maxcount < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetAttachedShaders: maxcount < 0");
    return;
  }
  typedef gles2::GetAttachedShaders::Result Result;
  uint32 size = Result::ComputeSize(maxcount);
  Result* result = transfer_buffer_.AllocTyped<Result>(size);
  result->SetNumResults(0);
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
  result->success = false;
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
  const char* result = NULL;
  // Clear the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetString(name, kResultBucketId);
  std::string str;
  if (GetBucketAsString(kResultBucketId, &str)) {
    // Because of WebGL the extensions can change. We have to cache each
    // unique result since we don't know when the client will stop referring to
    // a previous one it queries.
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

void GLES2Implementation::GetUniformfv(
    GLuint program, GLint location, GLfloat* params) {
  typedef gles2::GetUniformfv::Result Result;
  Result* result = static_cast<Result*>(result_buffer_);
  result->SetNumResults(0);
  helper_->GetUniformfv(
      program, location, result_shm_id(), result_shm_offset());
  WaitForCmd();
  result->CopyResult(params);
}

void GLES2Implementation::GetUniformiv(
    GLuint program, GLint location, GLint* params) {
  typedef gles2::GetUniformiv::Result Result;
  Result* result = static_cast<Result*>(result_buffer_);
  result->SetNumResults(0);
  helper_->GetUniformiv(
      program, location, result_shm_id(), result_shm_offset());
  WaitForCmd();
  static_cast<gles2::GetUniformfv::Result*>(result_buffer_)->CopyResult(params);
}

void GLES2Implementation::ReadPixels(
    GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format,
    GLenum type, void* pixels) {
  if (width < 0 || height < 0) {
    SetGLError(GL_INVALID_VALUE, "glReadPixels: dimensions < 0");
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
    SetGLError(GL_INVALID_VALUE, "glReadPixels: size too large.");
    return;
  }
  GLsizeiptr unpadded_row_size = temp_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, 2, format, type, pack_alignment_, &temp_size)) {
    SetGLError(GL_INVALID_VALUE, "glReadPixels: size too large.");
    return;
  }
  GLsizeiptr padded_row_size = temp_size - unpadded_row_size;
  if (padded_row_size < 0 || unpadded_row_size < 0) {
    SetGLError(GL_INVALID_VALUE, "glReadPixels: size too large.");
    return;
  }
  // Check if we have enough space to transfer at least an entire row.
  if (padded_row_size <= max_size) {
    // Transfer by rows.
    // The max rows we can transfer.
    GLint max_rows = max_size / std::max(padded_row_size,
                                         static_cast<GLsizeiptr>(1));
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
      if (*result != 0) {
        // We have to copy 1 row at a time to avoid writing pad bytes.
        const int8* src = static_cast<const int8*>(buffer);
        for (GLint yy = 0; yy < num_rows; ++yy) {
          memcpy(dest, src, unpadded_row_size);
          dest += padded_row_size;
          src += padded_row_size;
        }
      }
      transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
      // If it was not marked as successful exit.
      if (*result == 0) {
        return;
      }
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
        if (*result != 0) {
          memcpy(row_dest, buffer, part_size);
        }
        transfer_buffer_.FreePendingToken(buffer, helper_->InsertToken());
        // If it was not marked as successful exit.
        if (*result == 0) {
          return;
        }
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
bool GLES2Implementation::IsBufferReservedId(GLuint id) {
  for (size_t ii = 0; ii < arraysize(reserved_ids_); ++ii) {
    if (id == reserved_ids_[ii]) {
      return true;
    }
  }
  return false;
}
#else
bool GLES2Implementation::IsBufferReservedId(GLuint) {  // NOLINT
  return false;
}
#endif

#if defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)

void GLES2Implementation::BindBuffer(GLenum target, GLuint buffer) {
  if (IsBufferReservedId(buffer)) {
    SetGLError(GL_INVALID_OPERATION, "glBindBuffer: reserved buffer id");
    return;
  }
  buffer_id_handler_->MarkAsUsedForBind(buffer);
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
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glDeleteBuffers: n < 0");
    return;
  }
  buffer_id_handler_->FreeIds(n, buffers);
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
    SetGLError(GL_INVALID_VALUE, "glDrawArrays: count < 0");
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
      SetGLError(GL_INVALID_ENUM, "glGetVertexAttrib: invalid enum");
      break;
  }
  return true;
}

void GLES2Implementation::GetVertexAttribfv(
    GLuint index, GLenum pname, GLfloat* params) {
  uint32 value = 0;
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
  uint32 value = 0;
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

GLboolean GLES2Implementation::CommandBufferEnableCHROMIUM(
    const char* feature) {
  typedef CommandBufferEnableCHROMIUM::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  SetBucketAsCString(kResultBucketId, feature);
  helper_->CommandBufferEnableCHROMIUM(
      kResultBucketId, result_shm_id(), result_shm_offset());
  WaitForCmd();
  helper_->SetBucketSize(kResultBucketId, 0);
  return *result;
}

#endif  // defined(GLES2_SUPPORT_CLIENT_SIDE_BUFFERS)

void* GLES2Implementation::MapBufferSubDataCHROMIUM(
    GLuint target, GLintptr offset, GLsizeiptr size, GLenum access) {
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
  return mem;
}

void GLES2Implementation::UnmapBufferSubDataCHROMIUM(const void* mem) {
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
  if (!GLES2Util::ComputeImageDataSize(
      width, height, format, type, unpack_alignment_, &size)) {
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
  return mem;
}

void GLES2Implementation::UnmapTexSubImage2DCHROMIUM(const void* mem) {
  MappedTextureMap::iterator it = mapped_textures_.find(mem);
  if (it == mapped_textures_.end()) {
    SetGLError(
        GL_INVALID_VALUE, "UnmapTexSubImage2DCHROMIUM: texture not mapped");
    return;
  }
  const MappedTexture& mt = it->second;
  helper_->TexSubImage2D(
      mt.target, mt.level, mt.xoffset, mt.yoffset, mt.width, mt.height,
      mt.format, mt.type, mt.shm_id, mt.shm_offset);
  mapped_memory_->FreePendingToken(mt.shm_memory, helper_->InsertToken());
  mapped_textures_.erase(it);
}

const GLchar* GLES2Implementation::GetRequestableExtensionsCHROMIUM() {
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
  return reinterpret_cast<const GLchar*>(result);
}

void GLES2Implementation::RequestExtensionCHROMIUM(const char* extension) {
  SetBucketAsCString(kResultBucketId, extension);
  helper_->RequestExtensionCHROMIUM(kResultBucketId);
  helper_->SetBucketSize(kResultBucketId, 0);
}

}  // namespace gles2
}  // namespace gpu
