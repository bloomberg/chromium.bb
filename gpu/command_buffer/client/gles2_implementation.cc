// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class to emulate GLES2 over command buffers.

#include "../client/gles2_implementation.h"

#include <set>
#include <queue>
#include <GLES2/gl2ext.h>
#include "../client/mapped_memory.h"
#include "../client/program_info_manager.h"
#include "../common/gles2_cmd_utils.h"
#include "../common/id_allocator.h"
#include "../common/trace_event.h"

#if defined(__native_client__) && !defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
#define GLES2_SUPPORT_CLIENT_SIDE_ARRAYS
#endif

#if defined(GPU_CLIENT_DEBUG)
#include "ui/gfx/gl/gl_switches.h"
#include "base/command_line.h"
#endif

namespace gpu {
namespace gles2 {

// A 32-bit and 64-bit compatible way of converting a pointer to a GLuint.
static GLuint ToGLuint(const void* ptr) {
  return static_cast<GLuint>(reinterpret_cast<size_t>(ptr));
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
  virtual bool FreeIds(GLsizei n, const GLuint* ids) {
    for (GLsizei ii = 0; ii < n; ++ii) {
      id_allocator_.FreeID(ids[ii]);
    }
    return true;
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
  virtual bool FreeIds(GLsizei /* n */, const GLuint* /* ids */) {
    // Ids are never freed.
    return true;
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

  virtual bool FreeIds(GLsizei n, const GLuint* ids) {
    gles2_->DeleteSharedIdsCHROMIUM(id_namespace_, n, ids);
    // We need to ensure that the delete call is evaluated on the service side
    // before any other contexts issue commands using these client ids.
    gles2_->helper()->CommandBufferHelper::Flush();
    return true;
  }

  virtual bool MarkAsUsedForBind(GLuint /* id */) {
    // This has no meaning for shared resources.
    return true;
  }

 private:
  GLES2Implementation* gles2_;
  id_namespaces::IdNamespaces id_namespace_;
};

// An id handler for shared ids that requires ids are made before using and
// that only the context that created the id can delete it.
// Assumes the service will enforce that non made ids generate an error.
class StrictSharedIdHandler : public IdHandlerInterface {
 public:
  StrictSharedIdHandler(
    GLES2Implementation* gles2,
    id_namespaces::IdNamespaces id_namespace)
      : gles2_(gles2),
        id_namespace_(id_namespace) {
  }

  virtual ~StrictSharedIdHandler() { }

  virtual void MakeIds(GLuint id_offset, GLsizei n, GLuint* ids) {
    for (GLsizei ii = 0; ii < n; ++ii) {
      ids[ii] = GetId(id_offset);
    }
  }

  virtual bool FreeIds(GLsizei n, const GLuint* ids) {
    // OpenGL sematics. If any id is bad none of them get freed.
    for (GLsizei ii = 0; ii < n; ++ii) {
      GLuint id = ids[ii];
      if (id != 0) {
        ResourceIdSet::iterator it = used_ids_.find(id);
        if (it == used_ids_.end()) {
          return false;
        }
      }
    }
    for (GLsizei ii = 0; ii < n; ++ii) {
      GLuint id = ids[ii];
      if (id != 0) {
        ResourceIdSet::iterator it = used_ids_.find(id);
        if (it != used_ids_.end()) {
          used_ids_.erase(it);
          free_ids_.push(id);
        }
      }
    }
    return true;
  }

  virtual bool MarkAsUsedForBind(GLuint /* id */) {
    // This has no meaning for shared resources.
    return true;
  }

 private:
  static const GLsizei kNumIdsToGet = 2048;
  typedef std::queue<GLuint> ResourceIdQueue;
  typedef std::set<GLuint> ResourceIdSet;

  GLuint GetId(GLuint id_offset) {
    if (free_ids_.empty()) {
      GLuint ids[kNumIdsToGet];
      gles2_->GenSharedIdsCHROMIUM(id_namespace_, id_offset, kNumIdsToGet, ids);
      for (GLsizei ii = 0; ii < kNumIdsToGet; ++ii) {
        free_ids_.push(ids[ii]);
      }
    }
    GLuint id = free_ids_.front();
    free_ids_.pop();
    used_ids_.insert(id);
    return id;
  }

  bool FreeId(GLuint id) {
    ResourceIdSet::iterator it = used_ids_.find(id);
    if (it == used_ids_.end()) {
      return false;
    }
    used_ids_.erase(it);
    free_ids_.push(id);
    return true;
  }

  GLES2Implementation* gles2_;
  id_namespaces::IdNamespaces id_namespace_;
  ResourceIdSet used_ids_;
  ResourceIdQueue free_ids_;
};

#ifndef _MSC_VER
const GLsizei StrictSharedIdHandler::kNumIdsToGet;
#endif

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

AlignedRingBuffer::~AlignedRingBuffer() {
}

TransferBuffer::TransferBuffer(
    CommandBufferHelper* helper,
    int32 buffer_id,
    void* buffer,
    size_t buffer_size,
    size_t result_size,
    unsigned int alignment)
    : helper_(helper),
      buffer_size_(buffer_size),
      result_size_(result_size),
      alignment_(alignment) {
  Setup(buffer_id, buffer);
}

TransferBuffer::~TransferBuffer() {
}

void TransferBuffer::Setup(int32 buffer_id, void* buffer) {
  GPU_CHECK(!ring_buffer_.get());
  ring_buffer_.reset(new AlignedRingBuffer(
      alignment_,
      buffer_id,
      result_size_,
      buffer_size_ - result_size_,
      helper_,
      static_cast<char*>(buffer) + result_size_));
  buffer_id_ = buffer_id;
  result_buffer_ = buffer;
  result_shm_offset_ = 0;
}

void TransferBuffer::Free() {
  if (buffer_id_) {
    // TODO(gman): should we check the memory is unused?
    helper_->command_buffer()->DestroyTransferBuffer(buffer_id_);
    buffer_id_ = 0;
    result_buffer_ = NULL;
    result_shm_offset_ = 0;
    ring_buffer_.reset();
  }
}

void TransferBuffer::AllocateRingBuffer() {
  if (!buffer_id_) {
    int32 id = helper_->command_buffer()->CreateTransferBuffer(
        buffer_size_, -1);
    GPU_CHECK_NE(-1, id);  // if it fails we're done.
    gpu::Buffer shm = helper_->command_buffer()->GetTransferBuffer(id);
    Setup(id, shm.ptr);
  }
}

AlignedRingBuffer* TransferBuffer::GetBuffer() {
  AllocateRingBuffer();
  return ring_buffer_.get();
}

void* TransferBuffer::GetResultBuffer() {
  AllocateRingBuffer();
  GPU_CHECK(result_buffer_);
  return result_buffer_;
}

int TransferBuffer::GetResultOffset() {
  AllocateRingBuffer();
  return result_shm_offset_;
}

int TransferBuffer::GetShmId() {
  AllocateRingBuffer();
  GPU_CHECK(buffer_id_);
  return buffer_id_;
}

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
      bool share_resources,
      bool bind_generates_resource)
    : helper_(helper),
      transfer_buffer_(
          helper,
          transfer_buffer_id,
          transfer_buffer,
          transfer_buffer_size,
          GLES2Implementation::kStartingOffset,
          GLES2Implementation::kAlignment),
      angle_pack_reverse_row_order_status(kUnknownExtensionStatus),
      pack_alignment_(4),
      unpack_alignment_(4),
      unpack_flip_y_(false),
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
      sharing_resources_(share_resources),
      bind_generates_resource_(bind_generates_resource) {
  GPU_CLIENT_LOG_CODE_BLOCK({
    debug_ = CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableGPUClientLogging);
  });

  memset(&reserved_ids_, 0, sizeof(reserved_ids_));

  mapped_memory_.reset(new MappedMemoryManager(helper_));
  SetSharedMemoryChunkSizeMultiple(1024 * 1024 * 2);

  if (share_resources) {
    if (!bind_generates_resource) {
      for (int i = 0; i < id_namespaces::kNumIdNamespaces; ++i) {
        id_handlers_[i].reset(new StrictSharedIdHandler(
              this, static_cast<id_namespaces::IdNamespaces>(i)));
      }
    } else {
      for (int i = 0; i < id_namespaces::kNumIdNamespaces; ++i) {
        id_handlers_[i].reset(new SharedIdHandler(
              this, static_cast<id_namespaces::IdNamespaces>(i)));
      }
    }
  } else {
    for (int i = 0; i < id_namespaces::kNumIdNamespaces; ++i) {
      if (i == id_namespaces::kProgramsAndShaders)
        id_handlers_[i].reset(new NonSharedNonReusedIdHandler);
      else
        id_handlers_[i].reset(new NonSharedIdHandler);
    }
  }

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

  util_.set_num_compressed_texture_formats(
      gl_state_.num_compressed_texture_formats);
  util_.set_num_shader_binary_formats(
      gl_state_.num_shader_binary_formats);

  GetMultipleIntegervCHROMIUM(
      pnames, arraysize(pnames), &gl_state_.max_combined_texture_image_units,
      sizeof(gl_state_));

  texture_units_.reset(
      new TextureUnit[gl_state_.max_combined_texture_image_units]);

  program_info_manager_.reset(ProgramInfoManager::Create(sharing_resources_));

#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  id_handlers_[id_namespaces::kBuffers]->MakeIds(
      kClientSideArrayId, arraysize(reserved_ids_), &reserved_ids_[0]);

  client_side_buffer_helper_.reset(new ClientSideBufferHelper(
      gl_state_.max_vertex_attribs,
      reserved_ids_[0],
      reserved_ids_[1]));
#endif
}

GLES2Implementation::~GLES2Implementation() {
#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  DeleteBuffers(arraysize(reserved_ids_), &reserved_ids_[0]);
#endif
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
  transfer_buffer_.Free();
}

void GLES2Implementation::WaitForCmd() {
  TRACE_EVENT0("gpu", "GLES2::WaitForCmd");
  helper_->CommandBufferHelper::Finish();
}

namespace {
bool IsExtensionAvailable(GLES2Implementation* gles2, const char ext[]) {
  const char* extensions = reinterpret_cast<const char*>(
      gles2->GetString(GL_EXTENSIONS));
  int length = strlen(ext);
  while (true) {
    int n = strcspn(extensions, " ");
    if (n == length && 0 == strncmp(ext, extensions, length)) {
      return true;
    }
    if ('\0' == extensions[n]) {
      return false;
    }
    extensions += n+1;
  }
}
}

bool GLES2Implementation::IsAnglePackReverseRowOrderAvailable() {
  switch (angle_pack_reverse_row_order_status) {
    case kAvailableExtensionStatus:
      return true;
    case kUnavailableExtensionStatus:
      return false;
    default:
      if (IsExtensionAvailable(this, "GL_ANGLE_pack_reverse_row_order")) {
          angle_pack_reverse_row_order_status = kAvailableExtensionStatus;
          return true;
      } else {
          angle_pack_reverse_row_order_status = kUnavailableExtensionStatus;
          return false;
      }
  }
}

GLenum GLES2Implementation::GetError() {
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
  error_bits_ |= GLES2Util::GLErrorToErrorBit(error);
}

void GLES2Implementation::GetBucketContents(uint32 bucket_id,
                                            std::vector<int8>* data) {
  TRACE_EVENT0("gpu", "GLES2::GetBucketContents");
  GPU_DCHECK(data);
  typedef cmd::GetBucketSize::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = 0;
  helper_->GetBucketSize(bucket_id, GetResultShmId(), GetResultShmOffset());
  WaitForCmd();
  uint32 size = *result;
  data->resize(size);
  if (size > 0u) {
    AlignedRingBuffer* transfer_buffer = transfer_buffer_.GetBuffer();
    uint32 max_size = transfer_buffer->GetLargestFreeOrPendingSize();
    uint32 offset = 0;
    while (size) {
      uint32 part_size = std::min(max_size, size);
      void* buffer = transfer_buffer->Alloc(part_size);
      helper_->GetBucketData(
          bucket_id, offset, part_size,
          transfer_buffer->GetShmId(),
          transfer_buffer->GetOffset(buffer));
      WaitForCmd();
      memcpy(&(*data)[offset], buffer, part_size);
      transfer_buffer->FreePendingToken(buffer, helper_->InsertToken());
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
    AlignedRingBuffer* transfer_buffer = transfer_buffer_.GetBuffer();
    uint32 max_size = transfer_buffer->GetLargestFreeOrPendingSize();
    uint32 offset = 0;
    while (size) {
      uint32 part_size = std::min(static_cast<size_t>(max_size), size);
      void* buffer = transfer_buffer->Alloc(part_size);
      memcpy(buffer, static_cast<const int8*>(data) + offset, part_size);
      helper_->SetBucketData(
          bucket_id, offset, part_size,
          transfer_buffer->GetShmId(),
          transfer_buffer->GetOffset(buffer));
      transfer_buffer->FreePendingToken(buffer, helper_->InsertToken());
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

bool GLES2Implementation::GetHelper(GLenum pname, GLint* params) {
  switch (pname) {
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
      *params = gl_state_.max_combined_texture_image_units;
      return true;
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      *params = gl_state_.max_cube_map_texture_size;
      return true;
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      *params = gl_state_.max_fragment_uniform_vectors;
      return true;
    case GL_MAX_RENDERBUFFER_SIZE:
      *params = gl_state_.max_renderbuffer_size;
      return true;
    case GL_MAX_TEXTURE_IMAGE_UNITS:
      *params = gl_state_.max_texture_image_units;
      return true;
    case GL_MAX_TEXTURE_SIZE:
      *params = gl_state_.max_texture_size;
      return true;
    case GL_MAX_VARYING_VECTORS:
      *params = gl_state_.max_varying_vectors;
      return true;
    case GL_MAX_VERTEX_ATTRIBS:
      *params = gl_state_.max_vertex_attribs;
      return true;
    case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
      *params = gl_state_.max_vertex_texture_image_units;
      return true;
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
      *params = gl_state_.max_vertex_uniform_vectors;
      return true;
    case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
      *params = gl_state_.num_compressed_texture_formats;
      return true;
    case GL_NUM_SHADER_BINARY_FORMATS:
      *params = gl_state_.num_shader_binary_formats;
      return true;
    case GL_ARRAY_BUFFER_BINDING:
      if (bind_generates_resource_) {
        *params = bound_array_buffer_id_;
        return true;
      }
      return false;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
      if (bind_generates_resource_) {
        *params = bound_element_array_buffer_id_;
        return true;
      }
      return false;
    case GL_ACTIVE_TEXTURE:
      *params = active_texture_unit_ + GL_TEXTURE0;
      return true;
    case GL_TEXTURE_BINDING_2D:
      if (bind_generates_resource_) {
        *params = texture_units_[active_texture_unit_].bound_texture_2d;
        return true;
      }
      return false;
    case GL_TEXTURE_BINDING_CUBE_MAP:
      if (bind_generates_resource_) {
        *params = texture_units_[active_texture_unit_].bound_texture_cube_map;
        return true;
      }
      return false;
    case GL_FRAMEBUFFER_BINDING:
      if (bind_generates_resource_) {
        *params = bound_framebuffer_;
        return true;
      }
      return false;
    case GL_RENDERBUFFER_BINDING:
      if (bind_generates_resource_) {
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

void GLES2Implementation::DrawElements(
    GLenum mode, GLsizei count, GLenum type, const void* indices) {
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
  GPU_CLIENT_LOG("[" << this << "] glFlush()");
  // Insert the cmd to call glFlush
  helper_->Flush();
  // Flush our command buffer
  // (tell the service to execute up to the flush cmd.)
  helper_->CommandBufferHelper::Flush();
}

void GLES2Implementation::Finish() {
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
  AlignedRingBuffer* transfer_buffer = transfer_buffer_.GetBuffer();
  GLsizei max_size = transfer_buffer->GetLargestFreeOrPendingSize();
  GLsizei max_num_per = max_size / sizeof(ids[0]);
  while (n) {
    GLsizei num = std::min(n, max_num_per);
    GLint* id_buffer = transfer_buffer->AllocTyped<GLint>(num);
    helper_->GenSharedIdsCHROMIUM(
        namespace_id, id_offset, num,
        transfer_buffer->GetShmId(),
        transfer_buffer->GetOffset(id_buffer));
    WaitForCmd();
    memcpy(ids, id_buffer, sizeof(*ids) * num);
    transfer_buffer->FreePendingToken(id_buffer, helper_->InsertToken());
    n -= num;
    ids += num;
  }
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << ids[i]);
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
      GPU_CLIENT_LOG("  " << i << ": " << ids[i]);
    }
  });
  TRACE_EVENT0("gpu", "GLES2::DeleteSharedIdsCHROMIUM");
  AlignedRingBuffer* transfer_buffer = transfer_buffer_.GetBuffer();
  GLsizei max_size = transfer_buffer->GetLargestFreeOrPendingSize();
  GLsizei max_num_per = max_size / sizeof(ids[0]);
  while (n) {
    GLsizei num = std::min(n, max_num_per);
    GLint* id_buffer = transfer_buffer->AllocTyped<GLint>(num);
    memcpy(id_buffer, ids, sizeof(*ids) * num);
    helper_->DeleteSharedIdsCHROMIUM(
        namespace_id, num,
        transfer_buffer->GetShmId(),
        transfer_buffer->GetOffset(id_buffer));
    WaitForCmd();
    transfer_buffer->FreePendingToken(id_buffer, helper_->InsertToken());
    n -= num;
    ids += num;
  }
}

void GLES2Implementation::RegisterSharedIdsCHROMIUM(
    GLuint namespace_id, GLsizei n, const GLuint* ids) {
  GPU_CLIENT_LOG("[" << this << "] glRegisterSharedIdsCHROMIUM("
     << namespace_id << ", " << n << ", "
     << static_cast<const void*>(ids) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (GLsizei i = 0; i < n; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << ids[i]);
    }
  });
  TRACE_EVENT0("gpu", "GLES2::RegisterSharedIdsCHROMIUM");
  AlignedRingBuffer* transfer_buffer = transfer_buffer_.GetBuffer();
  GLsizei max_size = transfer_buffer->GetLargestFreeOrPendingSize();
  GLsizei max_num_per = max_size / sizeof(ids[0]);
  while (n) {
    GLsizei num = std::min(n, max_num_per);
    GLint* id_buffer = transfer_buffer->AllocTyped<GLint>(n);
    memcpy(id_buffer, ids, sizeof(*ids) * n);
    helper_->RegisterSharedIdsCHROMIUM(
        namespace_id, n,
        transfer_buffer->GetShmId(),
        transfer_buffer->GetOffset(id_buffer));
    WaitForCmd();
    transfer_buffer->FreePendingToken(id_buffer, helper_->InsertToken());
    n -= num;
    ids += num;
  }
}

void GLES2Implementation::BindAttribLocation(
  GLuint program, GLuint index, const char* name) {
  GPU_CLIENT_LOG("[" << this << "] glBindAttribLocation(" << program << ", "
      << index << ", " << name << ")");
  SetBucketAsString(kResultBucketId, name);
  helper_->BindAttribLocationBucket(program, index, kResultBucketId);
  helper_->SetBucketSize(kResultBucketId, 0);
}

void GLES2Implementation::GetVertexAttribPointerv(
    GLuint index, GLenum pname, void** ptr) {
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
  if (!id_handlers_[id_namespaces::kProgramsAndShaders]->FreeIds(1, &program)) {
    SetGLError(
        GL_INVALID_VALUE,
        "glDeleteProgram: id not created by this context.");
    return false;
  }
  program_info_manager_->DeleteInfo(program);
  helper_->DeleteProgram(program);
  return true;
}

bool GLES2Implementation::DeleteShaderHelper(GLuint shader) {
  if (!id_handlers_[id_namespaces::kProgramsAndShaders]->FreeIds(1, &shader)) {
    SetGLError(
        GL_INVALID_VALUE,
        "glDeleteShader: id not created by this context.");
    return false;
  }
  program_info_manager_->DeleteInfo(shader);
  helper_->DeleteShader(shader);
  return true;
}

GLint GLES2Implementation::GetAttribLocationHelper(
    GLuint program, const char* name) {
  typedef GetAttribLocationBucket::Result Result;
  Result* result = GetResultAs<Result*>();
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
  GPU_CLIENT_LOG("[" << this << "] glGetAttribLocation(" << program
      << ", " << name << ")");
  TRACE_EVENT0("gpu", "GLES2::GetAttribLocation");
  GLint loc = program_info_manager_->GetAttribLocation(this, program, name);
  GPU_CLIENT_LOG("returned " << loc);
  return loc;
}

GLint GLES2Implementation::GetUniformLocationHelper(
    GLuint program, const char* name) {
  typedef GetUniformLocationBucket::Result Result;
  Result* result = GetResultAs<Result*>();
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
  GPU_CLIENT_LOG("[" << this << "] glGetUniformLocation(" << program
      << ", " << name << ")");
  TRACE_EVENT0("gpu", "GLES2::GetUniformLocation");
  GLint loc = program_info_manager_->GetUniformLocation(this, program, name);
  GPU_CLIENT_LOG("returned " << loc);
  return loc;
}

bool GLES2Implementation::GetProgramivHelper(
    GLuint program, GLenum pname, GLint* params) {
  return program_info_manager_->GetProgramiv(this, program, pname, params);
}

void GLES2Implementation::LinkProgram(GLuint program) {
  GPU_CLIENT_LOG("[" << this << "] glLinkProgram(" << program << ")");
  helper_->LinkProgram(program);
  program_info_manager_->CreateInfo(program);
}

void GLES2Implementation::ShaderBinary(
    GLsizei n, const GLuint* shaders, GLenum binaryformat, const void* binary,
    GLsizei length) {
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
  GLsizei shader_id_size = n * sizeof(*shaders);
  AlignedRingBuffer* transfer_buffer = transfer_buffer_.GetBuffer();
  int8* buffer = transfer_buffer->AllocTyped<int8>(shader_id_size + length);
  void* shader_ids = buffer;
  void* shader_data = buffer + shader_id_size;
  memcpy(shader_ids, shaders, shader_id_size);
  memcpy(shader_data, binary, length);
  helper_->ShaderBinary(
      n,
      transfer_buffer->GetShmId(),
      transfer_buffer->GetOffset(shader_ids),
      binaryformat,
      transfer_buffer->GetShmId(),
      transfer_buffer->GetOffset(shader_data),
      length);
  transfer_buffer->FreePendingToken(buffer, helper_->InsertToken());
}

void GLES2Implementation::PixelStorei(GLenum pname, GLint param) {
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
    case GL_UNPACK_FLIP_Y_CHROMIUM:
        unpack_flip_y_ = (param != 0);
        return;
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

void GLES2Implementation::ShaderSource(
    GLuint shader, GLsizei count, const char** source, const GLint* length) {
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
  AlignedRingBuffer* transfer_buffer = transfer_buffer_.GetBuffer();
  uint32 max_size = transfer_buffer->GetLargestFreeOrPendingSize();
  uint32 offset = 0;
  for (GLsizei ii = 0; ii <= count; ++ii) {
    const char* src = ii < count ? source[ii] : "";
    if (src) {
      uint32 size = ii < count ?
          (length ? static_cast<size_t>(length[ii]) : strlen(src)) : 1;
      while (size) {
        uint32 part_size = std::min(size, max_size);
        void* buffer = transfer_buffer->Alloc(part_size);
        memcpy(buffer, src, part_size);
        helper_->SetBucketData(kResultBucketId, offset, part_size,
                               transfer_buffer->GetShmId(),
                               transfer_buffer->GetOffset(buffer));
        transfer_buffer->FreePendingToken(buffer, helper_->InsertToken());
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
  GPU_CLIENT_LOG("[" << this << "] glBufferData("
      << GLES2Util::GetStringBufferTarget(target) << ", "
      << size << ", "
      << static_cast<const void*>(data) << ", "
      << GLES2Util::GetStringBufferUsage(usage) << ")");
  AlignedRingBuffer* transfer_buffer = transfer_buffer_.GetBuffer();
  GLsizeiptr max_size = transfer_buffer->GetLargestFreeOrPendingSize();
  if (size > max_size || !data) {
    helper_->BufferData(target, size, 0, 0, usage);
    if (data != NULL) {
      BufferSubData(target, 0, size, data);
    }
    return;
  }

  void* buffer = transfer_buffer->Alloc(size);
  memcpy(buffer, data, size);
  helper_->BufferData(
      target,
      size,
      transfer_buffer->GetShmId(),
      transfer_buffer->GetOffset(buffer),
      usage);
  transfer_buffer->FreePendingToken(buffer, helper_->InsertToken());
}

void GLES2Implementation::BufferSubData(
    GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
  GPU_CLIENT_LOG("[" << this << "] glBufferSubData("
      << GLES2Util::GetStringBufferTarget(target) << ", "
      << offset << ", " << size << ", "
      << static_cast<const void*>(data) << ")");
  if (size == 0) {
    return;
  }

  if (size < 0) {
    SetGLError(GL_INVALID_VALUE, "glBufferSubData: size < 0");
    return;
  }

  const int8* source = static_cast<const int8*>(data);
  AlignedRingBuffer* transfer_buffer = transfer_buffer_.GetBuffer();
  GLsizeiptr max_size = transfer_buffer->GetLargestFreeOrPendingSize();
  while (size) {
    GLsizeiptr part_size = std::min(size, max_size);
    void* buffer = transfer_buffer->Alloc(part_size);
    memcpy(buffer, source, part_size);
    helper_->BufferSubData(target, offset, part_size,
                           transfer_buffer->GetShmId(),
                           transfer_buffer->GetOffset(buffer));
    transfer_buffer->FreePendingToken(buffer, helper_->InsertToken());
    offset += part_size;
    source += part_size;
    size -= part_size;
  }
}

void GLES2Implementation::CompressedTexImage2D(
    GLenum target, GLint level, GLenum internalformat, GLsizei width,
    GLsizei height, GLint border, GLsizei image_size, const void* data) {
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

bool GLES2Implementation::CopyRectToBufferFlipped(
    const void* pixels,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    void* buffer) {
  if (width == 0 || height == 0) {
    return true;
  }

  uint32 temp_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, 1, format, type, unpack_alignment_, &temp_size)) {
    SetGLError(GL_INVALID_VALUE, "glTexSubImage2D: size to large");
    return false;
  }
  GLsizeiptr unpadded_row_size = temp_size;
  if (!GLES2Util::ComputeImageDataSize(
      width, 2, format, type, unpack_alignment_, &temp_size)) {
    SetGLError(GL_INVALID_VALUE, "glTexSubImage2D: size to large");
    return false;
  }
  GLsizeiptr padded_row_size = temp_size - unpadded_row_size;
  if (padded_row_size < 0 || unpadded_row_size < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexSubImage2D: size to large");
    return false;
  }

  const int8* source = static_cast<const int8*>(pixels);
  int8* dest = static_cast<int8*>(buffer) + padded_row_size * (height - 1);
  for (; height; --height) {
    memcpy(dest, source, unpadded_row_size);
    dest -= padded_row_size;
    source += padded_row_size;
  }
  return true;
}

void GLES2Implementation::TexImage2D(
    GLenum target, GLint level, GLint internalformat, GLsizei width,
    GLsizei height, GLint border, GLenum format, GLenum type,
    const void* pixels) {
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
  if (!GLES2Util::ComputeImageDataSize(
      width, height, format, type, unpack_alignment_, &size)) {
    SetGLError(GL_INVALID_VALUE, "glTexImage2D: image size too large");
    return;
  }

  // Check if we can send it all at once.
  AlignedRingBuffer* transfer_buffer = transfer_buffer_.GetBuffer();
  unsigned int max_size = transfer_buffer->GetLargestFreeOrPendingSize();
  if (size > max_size || !pixels) {
    // No, so send it using TexSubImage2D.
    helper_->TexImage2D(
       target, level, internalformat, width, height, border, format, type,
       0, 0);
    if (pixels) {
      TexSubImage2DImpl(
          target, level, 0, 0, width, height, format, type, pixels, GL_TRUE);
    }
    return;
  }

  void* buffer = transfer_buffer->Alloc(size);
  bool copy_success = true;
  if (unpack_flip_y_) {
    copy_success = CopyRectToBufferFlipped(
        pixels, width, height, format, type, buffer);
  } else {
    memcpy(buffer, pixels, size);
  }

  if (copy_success) {
    helper_->TexImage2D(
        target, level, internalformat, width, height, border, format, type,
        transfer_buffer->GetShmId(), transfer_buffer->GetOffset(buffer));
  }
  transfer_buffer->FreePendingToken(buffer, helper_->InsertToken());
}

void GLES2Implementation::TexSubImage2D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void* pixels) {
  GPU_CLIENT_LOG("[" << this << "] glTexSubImage2D("
      << GLES2Util::GetStringTextureTarget(target) << ", "
      << level << ", "
      << xoffset << ", " << yoffset << ", "
      << width << ", " << height << ", "
      << GLES2Util::GetStringTextureFormat(format) << ", "
      << GLES2Util::GetStringPixelType(type) << ", "
      << static_cast<const void*>(pixels) << ")");
  TexSubImage2DImpl(
      target, level, xoffset, yoffset, width, height, format, type, pixels,
      GL_FALSE);
}

void GLES2Implementation::TexSubImage2DImpl(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
    GLsizei height, GLenum format, GLenum type, const void* pixels,
    GLboolean internal) {
  if (level < 0 || height < 0 || width < 0) {
    SetGLError(GL_INVALID_VALUE, "glTexSubImage2D dimension < 0");
    return;
  }
  if (height == 0 || width == 0) {
    return;
  }
  const int8* source = static_cast<const int8*>(pixels);
  AlignedRingBuffer* transfer_buffer = transfer_buffer_.GetBuffer();
  GLsizeiptr max_size = transfer_buffer->GetLargestFreeOrPendingSize();
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

  GLint original_yoffset = yoffset;
  if (padded_row_size <= max_size) {
    // Transfer by rows.
    GLint max_rows = max_size / std::max(padded_row_size,
                                         static_cast<GLsizeiptr>(1));
    while (height) {
      GLint num_rows = std::min(height, max_rows);
      GLsizeiptr part_size =
          (num_rows - 1) * padded_row_size + unpadded_row_size;
      void* buffer = transfer_buffer->Alloc(part_size);
      GLint y;
      if (unpack_flip_y_) {
        CopyRectToBufferFlipped(
            source, width, num_rows, format, type, buffer);
        // GPU_DCHECK(copy_success);  // can't check this because bot fails!
        y = original_yoffset + height - num_rows;
      } else {
        memcpy(buffer, source, part_size);
        y = yoffset;
      }
      helper_->TexSubImage2D(
          target, level, xoffset, y, width, num_rows, format, type,
          transfer_buffer->GetShmId(),
          transfer_buffer->GetOffset(buffer), internal);
      transfer_buffer->FreePendingToken(buffer, helper_->InsertToken());
      yoffset += num_rows;
      source += num_rows * padded_row_size;
      height -= num_rows;
    }
  } else {
    // Transfer by sub rows. Because GL has no maximum texture dimensions.
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
        void* buffer = transfer_buffer->Alloc(part_size);
        memcpy(buffer, row_source, part_size);
        GLint y = unpack_flip_y_ ? (original_yoffset + height - 1) : yoffset;
        helper_->TexSubImage2D(
            target, level, temp_xoffset, y, num_pixels, 1, format, type,
            transfer_buffer->GetShmId(),
            transfer_buffer->GetOffset(buffer), internal);
        transfer_buffer->FreePendingToken(buffer, helper_->InsertToken());
        row_source += part_size;
        temp_xoffset += num_pixels;
        temp_width -= num_pixels;
      }
      ++yoffset;
      source += padded_row_size;
    }
  }
}

bool GLES2Implementation::GetActiveAttribHelper(
    GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size,
    GLenum* type, char* name) {
  // Clear the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  typedef gles2::GetActiveAttrib::Result Result;
  Result* result = GetResultAs<Result*>();
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
  bool success = program_info_manager_->GetActiveAttrib(
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
  bool success = program_info_manager_->GetActiveUniform(
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
  AlignedRingBuffer* transfer_buffer = transfer_buffer_.GetBuffer();
  Result* result = transfer_buffer->AllocTyped<Result>(size);
  result->SetNumResults(0);
  helper_->GetAttachedShaders(
    program,
    transfer_buffer->GetShmId(),
    transfer_buffer->GetOffset(result),
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
  transfer_buffer->FreePendingToken(result, token);
}

void GLES2Implementation::GetShaderPrecisionFormat(
    GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision) {
  GPU_CLIENT_LOG("[" << this << "] glGetShaderPrecisionFormat("
      << GLES2Util::GetStringShaderType(shadertype) << ", "
      << GLES2Util::GetStringShaderPrecision(precisiontype) << ", "
      << static_cast<const void*>(range) << ", "
      << static_cast<const void*>(precision) << ", ");
  TRACE_EVENT0("gpu", "GLES2::GetShaderPrecisionFormat");
  typedef gles2::GetShaderPrecisionFormat::Result Result;
  Result* result = GetResultAs<Result*>();
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

const GLubyte* GLES2Implementation::GetString(GLenum name) {
  GPU_CLIENT_LOG("[" << this << "] glGetString("
      << GLES2Util::GetStringStringType(name) << ")");
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
            "GL_CHROMIUM_flipy";
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
  GPU_CLIENT_LOG("  returned " << static_cast<const char*>(result));
  return reinterpret_cast<const GLubyte*>(result);
}

void GLES2Implementation::GetUniformfv(
    GLuint program, GLint location, GLfloat* params) {
  GPU_CLIENT_LOG("[" << this << "] glGetUniformfv("
      << program << ", " << location << ", "
      << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2::GetUniformfv");
  typedef gles2::GetUniformfv::Result Result;
  Result* result = GetResultAs<Result*>();
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
  GPU_CLIENT_LOG("[" << this << "] glGetUniformiv("
      << program << ", " << location << ", "
      << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2::GetUniformiv");
  typedef gles2::GetUniformiv::Result Result;
  Result* result = GetResultAs<Result*>();
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
  Result* result = GetResultAs<Result*>();
  int8* dest = reinterpret_cast<int8*>(pixels);
  AlignedRingBuffer* transfer_buffer = transfer_buffer_.GetBuffer();
  GLsizeiptr max_size = transfer_buffer->GetLargestFreeOrPendingSize();
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
          unpadded_row_size + padded_row_size * (num_rows - 1);
      void* buffer = transfer_buffer->Alloc(part_size);
      *result = 0;  // mark as failed.
      helper_->ReadPixels(
          xoffset, yoffset, width, num_rows, format, type,
          transfer_buffer->GetShmId(), transfer_buffer->GetOffset(buffer),
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
        const int8* src = static_cast<const int8*>(buffer);
        for (GLint yy = 0; yy < num_rows; ++yy) {
          memcpy(rows_dst, src, unpadded_row_size);
          rows_dst += padded_row_size;
          src += padded_row_size;
        }
        if (!pack_reverse_row_order_) {
          dest = rows_dst;
        }
      }
      transfer_buffer->FreePendingToken(buffer, helper_->InsertToken());
      // If it was not marked as successful exit.
      if (*result == 0) {
        return;
      }
      yoffset += num_rows;
      height -= num_rows;
    }
  } else {
    // Transfer by sub rows. Because GL has no maximum texture dimensions.
    GLES2Util::ComputeImageDataSize(
       1, 1, format, type, pack_alignment_, &temp_size);
    GLsizeiptr element_size = temp_size;
    max_size -= max_size % element_size;
    GLint max_sub_row_pixels = max_size / element_size;
    if (pack_reverse_row_order_) {
      // start at the last row when flipping y.
      dest = dest + (height - 1) * padded_row_size;
    }
    for (; height; --height) {
      GLint temp_width = width;
      GLint temp_xoffset = xoffset;
      int8* row_dest = dest;
      while (temp_width) {
        GLint num_pixels = std::min(width, max_sub_row_pixels);
        GLsizeiptr part_size = num_pixels * element_size;
        void* buffer = transfer_buffer->Alloc(part_size);
        *result = 0;  // mark as failed.
        helper_->ReadPixels(
            temp_xoffset, yoffset, temp_width, 1, format, type,
            transfer_buffer->GetShmId(),
            transfer_buffer->GetOffset(buffer),
            GetResultShmId(), GetResultShmOffset());
        WaitForCmd();
        if (*result != 0) {
          memcpy(row_dest, buffer, part_size);
        }
        transfer_buffer->FreePendingToken(buffer, helper_->InsertToken());
        // If it was not marked as successful exit.
        if (*result == 0) {
          return;
        }
        row_dest += part_size;
        temp_xoffset += num_pixels;
        temp_width -= num_pixels;
      }
      ++yoffset;
      dest += pack_reverse_row_order_ ? -padded_row_size : padded_row_size;
    }
  }
}

void GLES2Implementation::ActiveTexture(GLenum texture) {
  GPU_CLIENT_LOG("[" << this << "] glActiveTexture("
      << GLES2Util::GetStringEnum(texture) << ")");
  GLuint texture_index = texture - GL_TEXTURE0;
  if (texture_index >=
      static_cast<GLuint>(gl_state_.max_combined_texture_image_units)) {
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
  id_handlers_[id_namespaces::kBuffers]->MarkAsUsedForBind(buffer);
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
  id_handlers_[id_namespaces::kFramebuffers]->MarkAsUsedForBind(framebuffer);
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
  id_handlers_[id_namespaces::kRenderbuffers]->MarkAsUsedForBind(renderbuffer);
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
  id_handlers_[id_namespaces::kTextures]->MarkAsUsedForBind(texture);
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
  if (!id_handlers_[id_namespaces::kBuffers]->FreeIds(n, buffers)) {
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
  helper_->DeleteBuffersImmediate(n, buffers);
}

void GLES2Implementation::DeleteFramebuffersHelper(
    GLsizei n, const GLuint* framebuffers) {
  if (!id_handlers_[id_namespaces::kFramebuffers]->FreeIds(n, framebuffers)) {
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
  helper_->DeleteFramebuffersImmediate(n, framebuffers);
}

void GLES2Implementation::DeleteRenderbuffersHelper(
    GLsizei n, const GLuint* renderbuffers) {
  if (!id_handlers_[id_namespaces::kRenderbuffers]->FreeIds(n, renderbuffers)) {
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
  helper_->DeleteRenderbuffersImmediate(n, renderbuffers);
}

void GLES2Implementation::DeleteTexturesHelper(
    GLsizei n, const GLuint* textures) {
  if (!id_handlers_[id_namespaces::kTextures]->FreeIds(n, textures)) {
    SetGLError(
        GL_INVALID_VALUE,
        "glDeleteTextures: id not created by this context.");
    return;
  }
  for (GLsizei ii = 0; ii < n; ++ii) {
    for (GLint tt = 0; tt < gl_state_.max_combined_texture_image_units; ++tt) {
      TextureUnit& unit = texture_units_[active_texture_unit_];
      if (textures[ii] == unit.bound_texture_2d) {
        unit.bound_texture_2d = 0;
      }
      if (textures[ii] == unit.bound_texture_cube_map) {
        unit.bound_texture_cube_map = 0;
      }
    }
  }
  helper_->DeleteTexturesImmediate(n, textures);
}

void GLES2Implementation::DisableVertexAttribArray(GLuint index) {
  GPU_CLIENT_LOG(
      "[" << this << "] glDisableVertexAttribArray(" << index << ")");
#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  client_side_buffer_helper_->SetAttribEnable(index, false);
#endif
  helper_->DisableVertexAttribArray(index);
}

void GLES2Implementation::EnableVertexAttribArray(GLuint index) {
  GPU_CLIENT_LOG("[" << this << "] glEnableVertexAttribArray(" << index << ")");
#if defined(GLES2_SUPPORT_CLIENT_SIDE_ARRAYS)
  client_side_buffer_helper_->SetAttribEnable(index, true);
#endif
  helper_->EnableVertexAttribArray(index);
}

void GLES2Implementation::DrawArrays(GLenum mode, GLint first, GLsizei count) {
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
    client_side_buffer_helper_->SetupSimualtedClientSideBuffers(
        this, helper_, first + count);
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
  GPU_CLIENT_LOG("[" << this << "] glEnableFeatureCHROMIUM("
                 << feature << ")");
  TRACE_EVENT0("gpu", "GLES2::EnableFeatureCHROMIUM");
  typedef EnableFeatureCHROMIUM::Result Result;
  Result* result = GetResultAs<Result*>();
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
  helper_->CommandBufferHelper::Flush();
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
  GPU_DCHECK(result.second);
  GPU_CLIENT_LOG("  returned " << mem);
  return mem;
}

void GLES2Implementation::UnmapTexSubImage2DCHROMIUM(const void* mem) {
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
  helper_->CommandBufferHelper::Flush();
  mapped_textures_.erase(it);
}

void GLES2Implementation::ResizeCHROMIUM(GLuint width, GLuint height) {
  GPU_CLIENT_LOG("[" << this << "] glResizeCHROMIUM("
                 << width << ", " << height << ")");
  helper_->ResizeCHROMIUM(width, height);
}

const GLchar* GLES2Implementation::GetRequestableExtensionsCHROMIUM() {
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
  AlignedRingBuffer* transfer_buffer = transfer_buffer_.GetBuffer();
  void* buffer = transfer_buffer->Alloc(size_needed);
  GLenum* pnames_buffer = static_cast<GLenum*>(buffer);
  void* results_buffer = pnames_buffer + count;
  memcpy(pnames_buffer, pnames, count * sizeof(GLenum));
  memset(results_buffer, 0, num_results * sizeof(GLint));
  helper_->GetMultipleIntegervCHROMIUM(
      transfer_buffer->GetShmId(),
      transfer_buffer->GetOffset(pnames_buffer),
      count,
      transfer_buffer->GetShmId(),
      transfer_buffer->GetOffset(results_buffer),
      size);
  WaitForCmd();
  memcpy(results, results_buffer, size);
  // TODO(gman): We should be able to free without a token.
  transfer_buffer->FreePendingToken(buffer, helper_->InsertToken());
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
  GPU_CLIENT_LOG("[" << this << "] CreateStreamTextureCHROMIUM("
      << texture << ")");
  TRACE_EVENT0("gpu", "GLES2::CreateStreamTextureCHROMIUM");
  typedef CreateStreamTextureCHROMIUM::Result Result;
  Result* result = GetResultAs<Result*>();
  *result = GL_ZERO;

  helper_->CreateStreamTextureCHROMIUM(texture,
                                       GetResultShmId(),
                                       GetResultShmOffset());
  WaitForCmd();

  return *result;
}

void GLES2Implementation::DestroyStreamTextureCHROMIUM(GLuint texture) {
  GPU_CLIENT_LOG("[" << this << "] DestroyStreamTextureCHROMIUM("
      << texture << ")");
  TRACE_EVENT0("gpu", "GLES2::DestroyStreamTextureCHROMIUM");
  helper_->DestroyStreamTextureCHROMIUM(texture);
}

void GLES2Implementation::PostSubBufferCHROMIUM(
    GLint x, GLint y, GLint width, GLint height) {
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

}  // namespace gles2
}  // namespace gpu
