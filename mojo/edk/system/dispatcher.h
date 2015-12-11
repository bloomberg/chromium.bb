// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_DISPATCHER_H_
#define MOJO_EDK_SYSTEM_DISPATCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/system/handle_signals_state.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/c/system/buffer.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace edk {

class Core;
class Dispatcher;
class DispatcherTransport;
class HandleTable;
class LocalMessagePipeEndpoint;
class PlatformSharedBufferMapping;
class ProxyMessagePipeEndpoint;
class TransportData;
class Awakable;

using DispatcherVector = std::vector<scoped_refptr<Dispatcher>>;

namespace test {

// Test helper. We need to declare it here so we can friend it.
MOJO_SYSTEM_IMPL_EXPORT DispatcherTransport
DispatcherTryStartTransport(Dispatcher* dispatcher);

}  // namespace test

// A |Dispatcher| implements Mojo primitives that are "attached" to a particular
// handle. This includes most (all?) primitives except for |MojoWait...()|. This
// object is thread-safe, with its state being protected by a single lock
// |lock_|, which is also made available to implementation subclasses (via the
// |lock()| method).
class MOJO_SYSTEM_IMPL_EXPORT Dispatcher
    : public base::RefCountedThreadSafe<Dispatcher> {
 public:
  enum class Type {
    UNKNOWN = 0,
    MESSAGE_PIPE,
    DATA_PIPE_PRODUCER,
    DATA_PIPE_CONSUMER,
    SHARED_BUFFER,
    WAIT_SET,

    // "Private" types (not exposed via the public interface):
    PLATFORM_HANDLE = -1
  };
  virtual Type GetType() const = 0;

  // These methods implement the various primitives named |Mojo...()|. These
  // take |lock_| and handle races with |Close()|. Then they call out to
  // subclasses' |...ImplNoLock()| methods (still under |lock_|), which actually
  // implement the primitives.
  // NOTE(vtl): This puts a big lock around each dispatcher (i.e., handle), and
  // prevents the various |...ImplNoLock()|s from releasing the lock as soon as
  // possible. If this becomes an issue, we can rethink this.
  MojoResult Close();

  // |transports| may be non-null if and only if there are handles to be
  // written; not that |this| must not be in |transports|. On success, all the
  // dispatchers in |transports| must have been moved to a closed state; on
  // failure, they should remain in their original state.
  MojoResult WriteMessage(const void* bytes,
                          uint32_t num_bytes,
                          std::vector<DispatcherTransport>* transports,
                          MojoWriteMessageFlags flags);
  // |dispatchers| must be non-null but empty, if |num_dispatchers| is non-null
  // and nonzero. On success, it will be set to the dispatchers to be received
  // (and assigned handles) as part of the message.
  MojoResult ReadMessage(void* bytes,
                         uint32_t* num_bytes,
                         DispatcherVector* dispatchers,
                         uint32_t* num_dispatchers,
                         MojoReadMessageFlags flags);
  MojoResult WriteData(const void* elements,
                       uint32_t* elements_num_bytes,
                       MojoWriteDataFlags flags);
  MojoResult BeginWriteData(void** buffer,
                            uint32_t* buffer_num_bytes,
                            MojoWriteDataFlags flags);
  MojoResult EndWriteData(uint32_t num_bytes_written);
  MojoResult ReadData(void* elements,
                      uint32_t* num_bytes,
                      MojoReadDataFlags flags);
  MojoResult BeginReadData(const void** buffer,
                           uint32_t* buffer_num_bytes,
                           MojoReadDataFlags flags);
  MojoResult EndReadData(uint32_t num_bytes_read);
  // |options| may be null. |new_dispatcher| must not be null, but
  // |*new_dispatcher| should be null (and will contain the dispatcher for the
  // new handle on success).
  MojoResult DuplicateBufferHandle(
      const MojoDuplicateBufferHandleOptions* options,
      scoped_refptr<Dispatcher>* new_dispatcher);
  MojoResult MapBuffer(
      uint64_t offset,
      uint64_t num_bytes,
      MojoMapBufferFlags flags,
      scoped_ptr<PlatformSharedBufferMapping>* mapping);

  // Gets the current handle signals state. (The default implementation simply
  // returns a default-constructed |HandleSignalsState|, i.e., no signals
  // satisfied or satisfiable.) Note: The state is subject to change from other
  // threads.
  HandleSignalsState GetHandleSignalsState() const;

  // Adds an awakable to this dispatcher, which will be woken up when this
  // object changes state to satisfy |signals| with context |context|. It will
  // also be woken up when it becomes impossible for the object to ever satisfy
  // |signals| with a suitable error status.
  //
  // If |signals_state| is non-null, on *failure* |*signals_state| will be set
  // to the current handle signals state (on success, it is left untouched).
  //
  // Returns:
  //  - |MOJO_RESULT_OK| if the awakable was added;
  //  - |MOJO_RESULT_ALREADY_EXISTS| if |signals| is already satisfied;
  //  - |MOJO_RESULT_INVALID_ARGUMENT| if the dispatcher has been closed; and
  //  - |MOJO_RESULT_FAILED_PRECONDITION| if it is not (or no longer) possible
  //    that |signals| will ever be satisfied.
  MojoResult AddAwakable(Awakable* awakable,
                         MojoHandleSignals signals,
                         uintptr_t context,
                         HandleSignalsState* signals_state);
  // Removes an awakable from this dispatcher. (It is valid to call this
  // multiple times for the same |awakable| on the same object, so long as
  // |AddAwakable()| was called at most once.) If |signals_state| is non-null,
  // |*signals_state| will be set to the current handle signals state.
  void RemoveAwakable(Awakable* awakable, HandleSignalsState* signals_state);

  // Adds a dispatcher to wait on. When the dispatcher satisfies |signals|, it
  // will be returned in the next call to |GetReadyDispatchers()|. If
  // |dispatcher| has been added, it must be removed before adding again,
  // otherwise |MOJO_RESULT_ALREADY_EXISTS| will be returned.
  MojoResult AddWaitingDispatcher(const scoped_refptr<Dispatcher>& dispatcher,
                                  MojoHandleSignals signals,
                                  uintptr_t context);
  // Removes a dispatcher to wait on. If |dispatcher| has not been added,
  // |MOJO_RESULT_NOT_FOUND| will be returned.
  MojoResult RemoveWaitingDispatcher(
      const scoped_refptr<Dispatcher>& dispatcher);
  // Returns a set of ready dispatchers. |*count| is the maximum number of
  // dispatchers to return, and will contain the number of dispatchers returned
  // in |dispatchers| on completion.
  MojoResult GetReadyDispatchers(uint32_t* count,
                                 DispatcherVector* dispatchers,
                                 MojoResult* results,
                                 uintptr_t* contexts);

  // A dispatcher must be put into a special state in order to be sent across a
  // message pipe. Outside of tests, only |HandleTableAccess| is allowed to do
  // this, since there are requirements on the handle table (see below).
  //
  // In this special state, only a restricted set of operations is allowed.
  // These are the ones available as |DispatcherTransport| methods. Other
  // |Dispatcher| methods must not be called until |DispatcherTransport::End()|
  // has been called.
  class HandleTableAccess {
   private:
    friend class Core;
    friend class HandleTable;
    // Tests also need this, to avoid needing |Core|.
    friend DispatcherTransport test::DispatcherTryStartTransport(Dispatcher*);

    // This must be called under the handle table lock and only if the handle
    // table entry is not marked busy. The caller must maintain a reference to
    // |dispatcher| until |DispatcherTransport::End()| is called.
    static DispatcherTransport TryStartTransport(Dispatcher* dispatcher);
  };

  // A |TransportData| may serialize dispatchers that are given to it (and which
  // were previously attached to the |MessageInTransit| that is creating it) and
  // and then (probably in a different process) deserialize.
  // Note that the |MessageInTransit| "owns" (i.e., has the only ref to) these
  // dispatchers, so there are no locking issues. (There's no lock ordering
  // issue, and in fact no need to take dispatcher locks at all.)
  // TODO(vtl): Consider making another wrapper similar to |DispatcherTransport|
  // (but with an owning, unique reference), and having
  // |CreateEquivalentDispatcherAndCloseImplNoLock()| return that wrapper (and
  // |MessageInTransit|, etc. only holding on to such wrappers).
  class TransportDataAccess {
   private:
    friend class TransportData;

    // Serialization API. These functions may only be called on such
    // dispatchers. See the |Dispatcher| methods of the same names for more
    // details.
    static void StartSerialize(Dispatcher* dispatcher,
                               size_t* max_size,
                               size_t* max_platform_handles);
    static bool EndSerializeAndClose(
        Dispatcher* dispatcher,
        void* destination,
        size_t* actual_size,
        PlatformHandleVector* platform_handles);

    // Deserialization API.
    // Note: This "clears" (i.e., reset to the invalid handle) any platform
    // handles that it takes ownership of.
    static scoped_refptr<Dispatcher> Deserialize(
        int32_t type,
        const void* source,
        size_t size,
        PlatformHandleVector* platform_handles);
  };

 protected:
  friend class base::RefCountedThreadSafe<Dispatcher>;
  friend class MessagePipeDispatcher;  // For TransportStarted/TransportEnded.

  Dispatcher();
  virtual ~Dispatcher();

  // These are to be overridden by subclasses (if necessary). They are called
  // exactly once -- first |CancelAllAwakablesNoLock()|, then
  // |CloseImplNoLock()|,
  // when the dispatcher is being closed. They are called under |lock_|.
  virtual void CancelAllAwakablesNoLock();
  virtual void CloseImplNoLock();
  virtual scoped_refptr<Dispatcher>
  CreateEquivalentDispatcherAndCloseImplNoLock() = 0;

  // These are to be overridden by subclasses (if necessary). They are never
  // called after the dispatcher has been closed. They are called under |lock_|.
  // See the descriptions of the methods without the "ImplNoLock" for more
  // information.
  virtual MojoResult WriteMessageImplNoLock(
      const void* bytes,
      uint32_t num_bytes,
      std::vector<DispatcherTransport>* transports,
      MojoWriteMessageFlags flags);
  virtual MojoResult ReadMessageImplNoLock(void* bytes,
                                           uint32_t* num_bytes,
                                           DispatcherVector* dispatchers,
                                           uint32_t* num_dispatchers,
                                           MojoReadMessageFlags flags);
  virtual MojoResult WriteDataImplNoLock(const void* elements,
                                         uint32_t* num_bytes,
                                         MojoWriteDataFlags flags);
  virtual MojoResult BeginWriteDataImplNoLock(
      void** buffer,
      uint32_t* buffer_num_bytes,
      MojoWriteDataFlags flags);
  virtual MojoResult EndWriteDataImplNoLock(uint32_t num_bytes_written);
  virtual MojoResult ReadDataImplNoLock(void* elements,
                                        uint32_t* num_bytes,
                                        MojoReadDataFlags flags);
  virtual MojoResult BeginReadDataImplNoLock(
      const void** buffer,
      uint32_t* buffer_num_bytes,
      MojoReadDataFlags flags);
  virtual MojoResult EndReadDataImplNoLock(uint32_t num_bytes_read);
  virtual MojoResult DuplicateBufferHandleImplNoLock(
      const MojoDuplicateBufferHandleOptions* options,
      scoped_refptr<Dispatcher>* new_dispatcher);
  virtual MojoResult MapBufferImplNoLock(
      uint64_t offset,
      uint64_t num_bytes,
      MojoMapBufferFlags flags,
      scoped_ptr<PlatformSharedBufferMapping>* mapping);
  virtual HandleSignalsState GetHandleSignalsStateImplNoLock() const;
  virtual MojoResult AddAwakableImplNoLock(Awakable* awakable,
                                           MojoHandleSignals signals,
                                           uintptr_t context,
                                           HandleSignalsState* signals_state);
  virtual void RemoveAwakableImplNoLock(Awakable* awakable,
                                        HandleSignalsState* signals_state);
  virtual MojoResult AddWaitingDispatcherImplNoLock(
      const scoped_refptr<Dispatcher>& dispatcher,
      MojoHandleSignals signals,
      uintptr_t context);
  virtual MojoResult RemoveWaitingDispatcherImplNoLock(
      const scoped_refptr<Dispatcher>& dispatcher);
  virtual MojoResult GetReadyDispatchersImplNoLock(
      uint32_t* count,
      DispatcherVector* dispatchers,
      MojoResult* results,
      uintptr_t* contexts);

  // These implement the API used to serialize dispatchers (described below).
  // They will only be called on a dispatcher that's attached to and "owned" by
  // a |MessageInTransit|. See the non-"impl" versions for more information.
  //
  // TODO(vtl): Consider making these pure virtual once most things support
  // being passed over a message pipe.
  virtual void StartSerializeImplNoLock(size_t* max_size,
                                        size_t* max_platform_handles);
  virtual bool EndSerializeAndCloseImplNoLock(
      void* destination,
      size_t* actual_size,
      PlatformHandleVector* platform_handles);

  // These are called before and after a dispatcher is being transported. i.e.
  // |TransportStarted| is called |StartSerializeImplNoLock| and
  // |TransportEnded| is called after |EndSerializeAndCloseImplNoLock|. They are
  // needed to avoid deadlocks when transporting a dispatcher. The reason is
  // that |StartSerializeImplNoLock| is called on the main thread and will lead
  // to calling RawChannel::ReleaseHandle. The dispatcher is locked and it will
  // acquire RawChannel's locks as well. however the RawChannel could have its
  // locks acquired on the IO thread and be waiting to acquire the dispatcher's
  // lock. To solve this deadlock, |TransportStarted| is called before the
  // dispatcher's lock is acquired.
  virtual void TransportStarted() {}
  virtual void TransportEnded() {}

  // This should be overridden to return true if/when there's an ongoing
  // operation (e.g., two-phase read/writes on data pipes) that should prevent a
  // handle from being sent over a message pipe (with status "busy").
  virtual bool IsBusyNoLock() const;

  // Available to subclasses. (Note: Returns a non-const reference, just like
  // |base::AutoLock|'s constructor takes a non-const reference.)
  base::Lock& lock() const { return lock_; }
  bool is_closed() const { return is_closed_; }

 private:
  friend class DispatcherTransport;

  // Closes the dispatcher. This must be done under lock, and unlike |Close()|,
  // the dispatcher must not be closed already. (This is the "equivalent" of
  // |CreateEquivalentDispatcherAndCloseNoLock()|, for situations where the
  // dispatcher must be disposed of instead of "transferred".)
  void CloseNoLock();

  // Creates an equivalent dispatcher -- representing the same resource as this
  // dispatcher -- and close (i.e., disable) this dispatcher. I.e., this
  // dispatcher will look as though it was closed, but the resource it
  // represents will be assigned to the new dispatcher. This must be called
  // under the dispatcher's lock.
  scoped_refptr<Dispatcher> CreateEquivalentDispatcherAndCloseNoLock();

  // API to serialize dispatchers, exposed to only |TransportData| (via
  // |TransportData|). They may only be called on a dispatcher attached to a
  // |MessageInTransit| (and in particular not in |CoreImpl|'s handle table).
  //
  // Starts the serialization. Returns (via the two "out" parameters) the
  // maximum amount of space that may be needed to serialize this dispatcher (no
  // more than |TransportData::kMaxSerializedDispatcherSize|) and the maximum
  // number of |PlatformHandle|s that may need to be attached. If this
  // dispatcher cannot be serialized, |*max_size| and |*max_platform_handles|
  // should be set to zero. A call to this method will ALWAYS be followed by a
  // call to |EndSerializeAndClose()| (even if this dispatcher cannot be
  // serialized).
  void StartSerialize(size_t* max_size,
                      size_t* max_platform_handles);
  // Completes the serialization of this dispatcher and closes it. (This call
  // will always follow an earlier call to |StartSerialize()|. This does so by
  // writing to |destination| and appending any |PlatformHandle|s needed to
  // |platform_handles| (which may be null if no platform handles were indicated
  // to be required to |StartSerialize()|). This may write no more than the
  // amount indicated by |StartSerialize()|. (WARNING: Beware of races, e.g., if
  // something can be mutated between the two calls!) Returns true on success,
  // in which case |*actual_size| is set to the amount it actually wrote to
  // |destination|. On failure, |*actual_size| should not be modified; however,
  // the dispatcher will still be closed.
  bool EndSerializeAndClose(void* destination,
                            size_t* actual_size,
                            PlatformHandleVector* platform_handles);

  // This protects the following members as well as any state added by
  // subclasses.
  mutable base::Lock lock_;
  bool is_closed_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(Dispatcher);
};

// Wrapper around a |Dispatcher| pointer, while it's being processed to be
// passed in a message pipe. See the comment about
// |Dispatcher::HandleTableAccess| for more details.
//
// Note: This class is deliberately "thin" -- no more expensive than a
// |Dispatcher*|.
class MOJO_SYSTEM_IMPL_EXPORT DispatcherTransport {
 public:
  DispatcherTransport() : dispatcher_(nullptr) {}

  void End();

  Dispatcher::Type GetType() const { return dispatcher_->GetType(); }
  bool IsBusy() const { return dispatcher_->IsBusyNoLock(); }
  void Close() { dispatcher_->CloseNoLock(); }
  scoped_refptr<Dispatcher> CreateEquivalentDispatcherAndClose() {
    return dispatcher_->CreateEquivalentDispatcherAndCloseNoLock();
  }

  bool is_valid() const { return !!dispatcher_; }

 protected:
  Dispatcher* dispatcher() { return dispatcher_; }

 private:
  friend class Dispatcher::HandleTableAccess;
  friend class MessagePipeDispatcher;  // For AttachTransportsNoLock.

  explicit DispatcherTransport(Dispatcher* dispatcher)
      : dispatcher_(dispatcher) {}

  Dispatcher* dispatcher_;

  // Copy and assign allowed.
};

// So logging macros and |DCHECK_EQ()|, etc. work.
MOJO_SYSTEM_IMPL_EXPORT inline std::ostream& operator<<(std::ostream& out,
                                                        Dispatcher::Type type) {
  return out << static_cast<int>(type);
}

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_DISPATCHER_H_
