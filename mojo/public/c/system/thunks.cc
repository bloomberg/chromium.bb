// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/c/system/thunks.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "mojo/public/c/system/core.h"

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/scoped_native_library.h"
#include "base/threading/thread_restrictions.h"
#endif

namespace {

MojoSystemThunks g_thunks = {0};

}  // namespace

namespace mojo {

// NOTE: This is defined within the global mojo namespace so that it can be
// referenced as a friend to base::ScopedAllowBlocking when library support is
// enabled.
class CoreLibraryInitializer {
 public:
  typedef void (*MojoGetSystemThunksFunction)(MojoSystemThunks* thunks);

  CoreLibraryInitializer() {
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
    auto environment = base::Environment::Create();

    base::FilePath library_path;
    std::string library_path_value;
    const char kLibraryPathEnvironmentVar[] = "MOJO_CORE_LIBRARY_PATH";
    if (environment->GetVar(kLibraryPathEnvironmentVar, &library_path_value)) {
      library_path = base::FilePath::FromUTF8Unsafe(library_path_value);
    } else {
      // Default to looking for the library in the current working directory.
      const base::FilePath::CharType kDefaultLibraryPathValue[] =
          FILE_PATH_LITERAL("./libmojo_core.so");
      library_path = base::FilePath(kDefaultLibraryPathValue);
    }

    base::ScopedAllowBlocking allow_blocking;
    library_.emplace(library_path);
    CHECK(library_->is_valid())
        << "Unable to load the mojo_core library. Make sure the library is in "
        << "the working directory or is correctly pointed to by the "
        << "MOJO_CORE_LIBRARY_PATH environment variable.";

    const char kGetThunksFunctionName[] = "MojoGetSystemThunks";
    MojoGetSystemThunksFunction get_thunks =
        reinterpret_cast<MojoGetSystemThunksFunction>(
            library_->GetFunctionPointer(kGetThunksFunctionName));
    CHECK(get_thunks) << "Invalid mojo_core library";

    DCHECK_EQ(g_thunks.size, 0u);
    g_thunks.size = sizeof(g_thunks);
    get_thunks(&g_thunks);

    CHECK_GT(g_thunks.size, 0u) << "Invalid mojo_core library";
#else   // defined(OS_CHROMEOS) || defined(OS_LINUX)
    NOTREACHED()
        << "Dynamic mojo_core loading is not supported on this platform.";
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)
  }

  ~CoreLibraryInitializer() = default;

 private:
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  base::Optional<base::ScopedNativeLibrary> library_;
#endif

  DISALLOW_COPY_AND_ASSIGN(CoreLibraryInitializer);
};

}  // namespace mojo

extern "C" {

MojoResult MojoInitialize(const struct MojoInitializeOptions* options) {
  static base::NoDestructor<mojo::CoreLibraryInitializer> initializer;
  ALLOW_UNUSED_LOCAL(initializer);
  DCHECK(g_thunks.Initialize);

  return g_thunks.Initialize(options);
}

MojoTimeTicks MojoGetTimeTicksNow() {
  return g_thunks.GetTimeTicksNow();
}

MojoResult MojoClose(MojoHandle handle) {
  return g_thunks.Close(handle);
}

MojoResult MojoQueryHandleSignalsState(
    MojoHandle handle,
    struct MojoHandleSignalsState* signals_state) {
  return g_thunks.QueryHandleSignalsState(handle, signals_state);
}

MojoResult MojoCreateMessagePipe(const MojoCreateMessagePipeOptions* options,
                                 MojoHandle* message_pipe_handle0,
                                 MojoHandle* message_pipe_handle1) {
  return g_thunks.CreateMessagePipe(options, message_pipe_handle0,
                                    message_pipe_handle1);
}

MojoResult MojoWriteMessage(MojoHandle message_pipe_handle,
                            MojoMessageHandle message_handle,
                            MojoWriteMessageFlags flags) {
  return g_thunks.WriteMessage(message_pipe_handle, message_handle, flags);
}

MojoResult MojoReadMessage(MojoHandle message_pipe_handle,
                           MojoMessageHandle* message_handle,
                           MojoReadMessageFlags flags) {
  assert(g_thunks.ReadMessage);
  return g_thunks.ReadMessage(message_pipe_handle, message_handle, flags);
}

MojoResult MojoCreateDataPipe(const MojoCreateDataPipeOptions* options,
                              MojoHandle* data_pipe_producer_handle,
                              MojoHandle* data_pipe_consumer_handle) {
  return g_thunks.CreateDataPipe(options, data_pipe_producer_handle,
                                 data_pipe_consumer_handle);
}

MojoResult MojoWriteData(MojoHandle data_pipe_producer_handle,
                         const void* elements,
                         uint32_t* num_elements,
                         MojoWriteDataFlags flags) {
  return g_thunks.WriteData(data_pipe_producer_handle, elements, num_elements,
                            flags);
}

MojoResult MojoBeginWriteData(MojoHandle data_pipe_producer_handle,
                              void** buffer,
                              uint32_t* buffer_num_elements,
                              MojoWriteDataFlags flags) {
  return g_thunks.BeginWriteData(data_pipe_producer_handle, buffer,
                                 buffer_num_elements, flags);
}

MojoResult MojoEndWriteData(MojoHandle data_pipe_producer_handle,
                            uint32_t num_elements_written) {
  return g_thunks.EndWriteData(data_pipe_producer_handle, num_elements_written);
}

MojoResult MojoReadData(MojoHandle data_pipe_consumer_handle,
                        void* elements,
                        uint32_t* num_elements,
                        MojoReadDataFlags flags) {
  return g_thunks.ReadData(data_pipe_consumer_handle, elements, num_elements,
                           flags);
}

MojoResult MojoBeginReadData(MojoHandle data_pipe_consumer_handle,
                             const void** buffer,
                             uint32_t* buffer_num_elements,
                             MojoReadDataFlags flags) {
  return g_thunks.BeginReadData(data_pipe_consumer_handle, buffer,
                                buffer_num_elements, flags);
}

MojoResult MojoEndReadData(MojoHandle data_pipe_consumer_handle,
                           uint32_t num_elements_read) {
  return g_thunks.EndReadData(data_pipe_consumer_handle, num_elements_read);
}

MojoResult MojoCreateSharedBuffer(
    const struct MojoCreateSharedBufferOptions* options,
    uint64_t num_bytes,
    MojoHandle* shared_buffer_handle) {
  return g_thunks.CreateSharedBuffer(options, num_bytes, shared_buffer_handle);
}

MojoResult MojoDuplicateBufferHandle(
    MojoHandle buffer_handle,
    const struct MojoDuplicateBufferHandleOptions* options,
    MojoHandle* new_buffer_handle) {
  return g_thunks.DuplicateBufferHandle(buffer_handle, options,
                                        new_buffer_handle);
}

MojoResult MojoMapBuffer(MojoHandle buffer_handle,
                         uint64_t offset,
                         uint64_t num_bytes,
                         void** buffer,
                         MojoMapBufferFlags flags) {
  return g_thunks.MapBuffer(buffer_handle, offset, num_bytes, buffer, flags);
}

MojoResult MojoUnmapBuffer(void* buffer) {
  return g_thunks.UnmapBuffer(buffer);
}

MojoResult MojoGetBufferInfo(MojoHandle buffer_handle,
                             const struct MojoSharedBufferOptions* options,
                             struct MojoSharedBufferInfo* info) {
  assert(g_thunks.GetBufferInfo);
  return g_thunks.GetBufferInfo(buffer_handle, options, info);
}

MojoResult MojoCreateTrap(MojoTrapEventHandler handler,
                          const MojoCreateTrapOptions* options,
                          MojoHandle* trap_handle) {
  return g_thunks.CreateTrap(handler, options, trap_handle);
}

MojoResult MojoAddTrigger(MojoHandle trap_handle,
                          MojoHandle handle,
                          MojoHandleSignals signals,
                          MojoTriggerCondition condition,
                          uintptr_t context,
                          const MojoAddTriggerOptions* options) {
  return g_thunks.AddTrigger(trap_handle, handle, signals, condition, context,
                             options);
}

MojoResult MojoRemoveTrigger(MojoHandle trap_handle,
                             uintptr_t context,
                             const MojoRemoveTriggerOptions* options) {
  return g_thunks.RemoveTrigger(trap_handle, context, options);
}

MojoResult MojoArmTrap(MojoHandle trap_handle,
                       const MojoArmTrapOptions* options,
                       uint32_t* num_ready_triggers,
                       uintptr_t* ready_triggers,
                       MojoResult* ready_results,
                       MojoHandleSignalsState* ready_signals_states) {
  return g_thunks.ArmTrap(trap_handle, options, num_ready_triggers,
                          ready_triggers, ready_results, ready_signals_states);
}

MojoResult MojoFuseMessagePipes(MojoHandle handle0, MojoHandle handle1) {
  return g_thunks.FuseMessagePipes(handle0, handle1);
}

MojoResult MojoCreateMessage(const MojoCreateMessageOptions* options,
                             MojoMessageHandle* message) {
  return g_thunks.CreateMessage(options, message);
}

MojoResult MojoDestroyMessage(MojoMessageHandle message) {
  return g_thunks.DestroyMessage(message);
}

MojoResult MojoSerializeMessage(MojoMessageHandle message,
                                const MojoSerializeMessageOptions* options) {
  return g_thunks.SerializeMessage(message, options);
}

MojoResult MojoAppendMessageData(MojoMessageHandle message,
                                 uint32_t payload_size,
                                 const MojoHandle* handles,
                                 uint32_t num_handles,
                                 const MojoAppendMessageDataOptions* options,
                                 void** buffer,
                                 uint32_t* buffer_size) {
  return g_thunks.AppendMessageData(message, payload_size, handles, num_handles,
                                    options, buffer, buffer_size);
}

MojoResult MojoGetMessageData(MojoMessageHandle message,
                              const MojoGetMessageDataOptions* options,
                              void** buffer,
                              uint32_t* num_bytes,
                              MojoHandle* handles,
                              uint32_t* num_handles) {
  return g_thunks.GetMessageData(message, options, buffer, num_bytes, handles,
                                 num_handles);
}

MojoResult MojoSetMessageContext(MojoMessageHandle message,
                                 uintptr_t context,
                                 MojoMessageContextSerializer serializer,
                                 MojoMessageContextDestructor destructor,
                                 const MojoSetMessageContextOptions* options) {
  return g_thunks.SetMessageContext(message, context, serializer, destructor,
                                    options);
}

MojoResult MojoGetMessageContext(MojoMessageHandle message,
                                 const MojoGetMessageContextOptions* options,
                                 uintptr_t* context) {
  return g_thunks.GetMessageContext(message, options, context);
}

MojoResult MojoWrapPlatformHandle(
    const struct MojoPlatformHandle* platform_handle,
    MojoHandle* mojo_handle) {
  return g_thunks.WrapPlatformHandle(platform_handle, mojo_handle);
}

MojoResult MojoUnwrapPlatformHandle(
    MojoHandle mojo_handle,
    struct MojoPlatformHandle* platform_handle) {
  return g_thunks.UnwrapPlatformHandle(mojo_handle, platform_handle);
}

MojoResult MojoWrapPlatformSharedBufferHandle(
    const struct MojoPlatformHandle* platform_handle,
    size_t num_bytes,
    const struct MojoSharedBufferGuid* guid,
    MojoPlatformSharedBufferHandleFlags flags,
    MojoHandle* mojo_handle) {
  return g_thunks.WrapPlatformSharedBufferHandle(platform_handle, num_bytes,
                                                 guid, flags, mojo_handle);
}

MojoResult MojoUnwrapPlatformSharedBufferHandle(
    MojoHandle mojo_handle,
    struct MojoPlatformHandle* platform_handle,
    size_t* num_bytes,
    struct MojoSharedBufferGuid* guid,
    MojoPlatformSharedBufferHandleFlags* flags) {
  return g_thunks.UnwrapPlatformSharedBufferHandle(mojo_handle, platform_handle,
                                                   num_bytes, guid, flags);
}

MojoResult MojoNotifyBadMessage(MojoMessageHandle message,
                                const char* error,
                                size_t error_num_bytes) {
  return g_thunks.NotifyBadMessage(message, error, error_num_bytes);
}

MojoResult MojoGetProperty(MojoPropertyType type, void* value) {
  return g_thunks.GetProperty(type, value);
}

}  // extern "C"

void MojoEmbedderSetSystemThunks(const MojoSystemThunks* thunks) {
  // Assume embedders will always use matching versions of the EDK and public
  // APIs.
  DCHECK_EQ(thunks->size, sizeof(g_thunks));

  // This should only have to check that the |g_thunks.size| is zero, but we
  // have multiple EDK initializations in some test suites still. For now we
  // allow double calls as long as they're the same thunks as before.
  DCHECK(g_thunks.size == 0 || !memcmp(&g_thunks, thunks, sizeof(g_thunks)))
      << "Cannot set embedder thunks after Mojo API calls have been made.";

  g_thunks = *thunks;
}
