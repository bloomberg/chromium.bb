// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/handle_watcher.h"

#include <map>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "mojo/common/scoped_message_pipe.h"

namespace mojo {
namespace common {

typedef int WatcherID;

namespace {

const char kWatcherThreadName[] = "handle-watcher-thread";

// WatcherThreadManager --------------------------------------------------------

// WatcherThreadManager manages listening for the handles. It is a singleton. It
// spawns a thread that waits for any handle passed to StartWatching() to be
// ready.  Additionally it creates a message pipe for communication between the
// two threads. The message pipe is used solely to wake up the background
// thread.  This happens when the set of handles changes, or during shutdown.
class WatcherThreadManager {
 public:
  // Returns the shared instance.
  static WatcherThreadManager* GetInstance();

  // Starts watching the requested handle. Returns a unique ID that is used to
  // stop watching the handle. When the handle is ready |callback| is notified
  // on the thread StartWatching() was invoked on.
  // This may be invoked on any thread.
  WatcherID StartWatching(MojoHandle handle,
                          MojoWaitFlags wait_flags,
                          base::TimeTicks deadline,
                          const base::Callback<void(MojoResult)>& callback);

  // Stops watching a handle.
  void StopWatching(WatcherID watcher_id);

 private:
  friend struct base::DefaultLazyInstanceTraits<WatcherThreadManager>;

  // Tracks a single request.
  struct HandleAndCallback {
    HandleAndCallback()
        : handle(MOJO_HANDLE_INVALID),
          wait_flags(MOJO_WAIT_FLAG_NONE),
          message_loop(NULL) {}

    MojoHandle handle;
    MojoWaitFlags wait_flags;
    base::TimeTicks deadline;
    base::Callback<void(MojoResult)> callback;
    scoped_refptr<base::MessageLoopProxy> message_loop;
  };

  // Contains the state needed for MojoWaitMany.
  // NOTE: |handles| and |wait_flags| are separate vectors to make it easy to
  // pass to MojoWaitMany.
  struct WaitState {
    // List of ids.
    std::vector<WatcherID> ids;

    // List of handles.
    std::vector<MojoHandle> handles;

    // List of flags each handle is waiting on.
    std::vector<MojoWaitFlags> wait_flags;

    // First deadline.
    MojoDeadline deadline;

    // Set of ids whose deadline has been reached.
    std::set<WatcherID> deadline_exceeded;
  };

  typedef std::map<WatcherID, HandleAndCallback> IDToCallbackMap;

  WatcherThreadManager();
  ~WatcherThreadManager();

  // Invoked on the background thread. Runs a loop waiting on current set of
  // handles.
  void RunOnBackgroundThread();

  // Writes to the communication pipe to wake up the background thread.
  void SignalBackgroundThread();

  // Invoked when a handle associated with |id| should be removed and notified.
  // |result| gives the reason for removing.
  void RemoveAndNotify(WatcherID id, MojoResult result);

  // Removes all callbacks schedule for |handle|. This is used when a handle
  // is identified as invalid.
  void RemoveHandle(MojoHandle handle);

  MojoHandle read_handle() const { return control_pipe_.handle_0(); }
  MojoHandle write_handle() const { return control_pipe_.handle_1(); }

  // Returns state needed for MojoWaitMany.
  WaitState GetWaitState();

  // Guards members accessed on both threads.
  base::Lock lock_;

  // Used for communicating with the background thread.
  ScopedMessagePipe control_pipe_;

  base::Thread thread_;

  // Maps from assigned id to the callback.
  IDToCallbackMap id_to_callback_;

  // If true the background loop should exit.
  bool quit_;

  DISALLOW_COPY_AND_ASSIGN(WatcherThreadManager);
};

WatcherThreadManager* WatcherThreadManager::GetInstance() {
  static base::LazyInstance<WatcherThreadManager> instance =
      LAZY_INSTANCE_INITIALIZER;
  return &instance.Get();
}

WatcherID WatcherThreadManager::StartWatching(
    MojoHandle handle,
    MojoWaitFlags wait_flags,
    base::TimeTicks deadline,
    const base::Callback<void(MojoResult)>& callback) {
  WatcherID id = 0;
  {
    static int next_id = 0;
    base::AutoLock lock(lock_);
    // TODO(sky): worry about overflow?
    id = ++next_id;
    id_to_callback_[id].handle = handle;
    id_to_callback_[id].callback = callback;
    id_to_callback_[id].wait_flags = wait_flags;
    id_to_callback_[id].deadline = deadline;
    id_to_callback_[id].message_loop = base::MessageLoopProxy::current();
  }
  SignalBackgroundThread();
  return id;
}


void WatcherThreadManager::StopWatching(WatcherID watcher_id) {
  {
    base::AutoLock lock(lock_);
    // It's possible we've already serviced the handle but HandleWatcher hasn't
    // processed it yet.
    IDToCallbackMap::iterator i = id_to_callback_.find(watcher_id);
    if (i == id_to_callback_.end())
      return;
    id_to_callback_.erase(i);
  }
  SignalBackgroundThread();
}

WatcherThreadManager::WatcherThreadManager()
    : thread_(kWatcherThreadName),
      quit_(false) {
  // TODO(sky): deal with error condition?
  CHECK_NE(MOJO_HANDLE_INVALID, read_handle());
  thread_.Start();
  thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&WatcherThreadManager::RunOnBackgroundThread,
                 base::Unretained(this)));
}

WatcherThreadManager::~WatcherThreadManager() {
  {
    base::AutoLock lock(lock_);
    quit_ = true;
  }
  SignalBackgroundThread();

  thread_.Stop();
}

void WatcherThreadManager::RunOnBackgroundThread() {
  while (true) {
    const WaitState state = GetWaitState();
    for (std::set<WatcherID>::const_iterator i =
             state.deadline_exceeded.begin();
         i != state.deadline_exceeded.end(); ++i) {
      RemoveAndNotify(*i, MOJO_RESULT_DEADLINE_EXCEEDED);
    }
    const MojoResult result = MojoWaitMany(&state.handles.front(),
                                           &state.wait_flags.front(),
                                           state.handles.size(),
                                           state.deadline);

    if (result >= 0) {
      DCHECK_LT(result, static_cast<int>(state.handles.size()));
      // Last handle is used to wake us up.
      if (result == static_cast<int>(state.handles.size()) - 1) {
        uint32_t num_bytes = 0;
        MojoReadMessage(read_handle(), NULL, &num_bytes, NULL, 0,
                        MOJO_READ_MESSAGE_FLAG_MAY_DISCARD);
        {
          base::AutoLock lock(lock_);
          if (quit_)
            return;
        }
      } else {
        RemoveAndNotify(state.ids[result], MOJO_RESULT_OK);
      }
    } else if (result == MOJO_RESULT_INVALID_ARGUMENT ||
               result == MOJO_RESULT_FAILED_PRECONDITION) {
      // One of the handles is invalid or the flags supplied is invalid, remove
      // it.
      // Use -1 as last handle is used for communication and should never be
      // invalid.
      for (size_t i = 0; i < state.handles.size() - 1; ++i) {
        const MojoResult result =
            MojoWait(state.handles[i], state.wait_flags[i], 0);
        switch (result) {
          // TODO: do we really want to notify for all these conditions?
          case MOJO_RESULT_OK:
          case MOJO_RESULT_FAILED_PRECONDITION:
          case MOJO_RESULT_INVALID_ARGUMENT:
            RemoveAndNotify(state.ids[i], result);
            break;
          case MOJO_RESULT_DEADLINE_EXCEEDED:
            break;
          default:
            NOTREACHED();
        }
      }
    }
  }
}

void WatcherThreadManager::SignalBackgroundThread() {
  // TODO(sky): deal with error?
  MojoWriteMessage(write_handle(), NULL, 0, NULL, 0,
                   MOJO_WRITE_MESSAGE_FLAG_NONE);
}

void WatcherThreadManager::RemoveAndNotify(WatcherID id, MojoResult result) {
  HandleAndCallback to_notify;
  {
    base::AutoLock lock(lock_);
    IDToCallbackMap::iterator i = id_to_callback_.find(id);
    if (i == id_to_callback_.end())
      return;
    to_notify = i->second;
    id_to_callback_.erase(i);
  }
  to_notify.message_loop->PostTask(FROM_HERE,
                                   base::Bind(to_notify.callback, result));
}

void WatcherThreadManager::RemoveHandle(MojoHandle handle) {
  {
    base::AutoLock lock(lock_);
    for (IDToCallbackMap::iterator i = id_to_callback_.begin();
         i != id_to_callback_.end(); ) {
      if (i->second.handle == handle) {
        id_to_callback_.erase(i++);
      } else {
        ++i;
      }
    }
  }
}

WatcherThreadManager::WaitState WatcherThreadManager::GetWaitState() {
  WaitState state;
  const base::TimeTicks now(HandleWatcher::NowTicks());
  base::TimeDelta deadline;
  {
    base::AutoLock lock(lock_);
    for (IDToCallbackMap::const_iterator i = id_to_callback_.begin();
         i != id_to_callback_.end(); ++i) {
      if (!i->second.deadline.is_null()) {
        if (i->second.deadline <= now) {
          state.deadline_exceeded.insert(i->first);
          continue;
        } else {
          const base::TimeDelta delta = i->second.deadline - now;
          if (deadline == base::TimeDelta() || delta < deadline)
            deadline = delta;
        }
      }
      state.ids.push_back(i->first);
      state.handles.push_back(i->second.handle);
      state.wait_flags.push_back(i->second.wait_flags);
    }
  }
  state.ids.push_back(0);
  state.handles.push_back(read_handle());
  state.wait_flags.push_back(MOJO_WAIT_FLAG_READABLE);
  state.deadline = (deadline == base::TimeDelta()) ?
      MOJO_DEADLINE_INDEFINITE : deadline.InMicroseconds();
  return state;
}

}  // namespace

// HandleWatcher::StartState ---------------------------------------------------

// Contains the information passed to Start().
struct HandleWatcher::StartState {
  explicit StartState(HandleWatcher* watcher) : weak_factory(watcher) {
  }

  ~StartState() {
  }

  // ID assigned by WatcherThreadManager.
  WatcherID watcher_id;

  // Callback to notify when done.
  base::Callback<void(MojoResult)> callback;

  // When Start() is invoked a callback is passed to WatcherThreadManager
  // using a WeakRef from |weak_refactory_|. The callback invokes
  // OnHandleReady() (on the thread Start() is invoked from) which in turn
  // notifies |callback_|. Doing this allows us to reset state when the handle
  // is ready, and then notify the callback. Doing this also means Stop()
  // cancels any pending callbacks that may be inflight.
  base::WeakPtrFactory<HandleWatcher> weak_factory;
};

// HandleWatcher ---------------------------------------------------------------

// static
base::TickClock* HandleWatcher::tick_clock_ = NULL;

HandleWatcher::HandleWatcher() {
}

HandleWatcher::~HandleWatcher() {
  Stop();
}

void HandleWatcher::Start(MojoHandle handle,
                          MojoWaitFlags wait_flags,
                          MojoDeadline deadline,
                          const base::Callback<void(MojoResult)>& callback) {
  DCHECK_NE(MOJO_HANDLE_INVALID, handle);
  DCHECK_NE(MOJO_WAIT_FLAG_NONE, wait_flags);

  Stop();

  start_state_.reset(new StartState(this));
  start_state_->callback = callback;
  start_state_->watcher_id =
      WatcherThreadManager::GetInstance()->StartWatching(
          handle,
          wait_flags,
          MojoDeadlineToTimeTicks(deadline),
          base::Bind(&HandleWatcher::OnHandleReady,
                     start_state_->weak_factory.GetWeakPtr()));
}

void HandleWatcher::Stop() {
  if (!start_state_.get())
    return;

  scoped_ptr<StartState> old_state(start_state_.Pass());
  WatcherThreadManager::GetInstance()->StopWatching(old_state->watcher_id);
}

void HandleWatcher::OnHandleReady(MojoResult result) {
  DCHECK(start_state_.get());
  scoped_ptr<StartState> old_state(start_state_.Pass());
  old_state->callback.Run(result);

  // NOTE: We may have been deleted during callback execution.
}

// static
base::TimeTicks HandleWatcher::NowTicks() {
  return tick_clock_ ? tick_clock_->NowTicks() : base::TimeTicks::Now();
}

// static
base::TimeTicks HandleWatcher::MojoDeadlineToTimeTicks(MojoDeadline deadline) {
  return deadline == MOJO_DEADLINE_INDEFINITE ? base::TimeTicks() :
      NowTicks() + base::TimeDelta::FromMicroseconds(deadline);
}

}  // namespace common
}  // namespace mojo
