// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// ExecutorsManager implementation, an object to keep track of the
// CeeeExecutor objects that were instantiated in destination threads.

#ifndef CEEE_IE_BROKER_EXECUTORS_MANAGER_H_
#define CEEE_IE_BROKER_EXECUTORS_MANAGER_H_

#include <atlbase.h>
#include <atlcom.h>
#include <map>
#include <list>

#include "base/singleton.h"
#include "base/synchronization/lock.h"
#include "base/task.h"
#include "ceee/common/window_utils.h"
#include "ceee/ie/broker/window_events_funnel.h"

#include "toolband.h"  // NOLINT

// This class is to be used as a single instance for the broker module to
// hold on a map of executor objects per thread that won't go away when
// the instance of the Broker object does.
//
// See the @ref ExecutorsManagerDoc page for more details.

// Manages a map of destination threads to CeeeExecutor interfaces.
class ExecutorsManager {
 public:
  // Identifiers for destination threads where to run executors.
  typedef DWORD ThreadId;

  // Returns the singleton instance.
  static ExecutorsManager* GetInstance();

  // To avoid lint errors, even though we are only virtual for unittests.
  virtual ~ExecutorsManager() {}

  // Temporary until we have a better structure to recognize known windows.
  // TODO(mad@chromium.org): Find a better structure.
  static bool IsKnownWindow(HWND window);
  virtual bool IsKnownWindowImpl(HWND window);
  static HWND FindTabChild(HWND window);
  virtual HWND FindTabChildImpl(HWND window);

  // Adds a new executor to the map associated to the given thread_id.
  //
  // @param thread_id The thread for which we want to register a new executor.
  // @param executor  The executor we want to register for the given thread_id.
  // @return S_OK iff we didn't already have an executor, and we had a pending
  //              request to add one for that exact same thread.
  virtual HRESULT RegisterWindowExecutor(ThreadId thread_id,
                                         IUnknown* executor);
  // TODO(mad@chromium.org): Implement the proper manual/secure registration.
  //
  // @param thread_id The thread for which we want to register a new executor.
  // @param executor  The executor we want to register for the given thread_id.
  // @return S_OK iff we didn't already have an executor, and we had a pending
  //              request to add one for that exact same thread.
  virtual HRESULT RegisterTabExecutor(ThreadId thread_id, IUnknown* executor);

  // Gets the executor associated to the given thread_id. Gets if from the map
  // if there was already one in there or create a new one otherwise.
  //
  // @param thread_id The thread for which we want the executor.
  // @param window The window handle for which we want the executor.
  // @param riid Which interface is to be returned in @p executor.
  // @param executor  Where to return the pointer to that executor.
  // @return S_OK iff we found an existing or successfully created an executor.
  virtual HRESULT GetExecutor(ThreadId thread_id, HWND window, REFIID riid,
                              void** executor);

  // Removes an executor from our map.
  //
  // @param thread_id The thread for which we want to remove the executor.
  // @return S_OK if we removed the executor or S_FALSE if it wasn't there.
  virtual HRESULT RemoveExecutor(ThreadId thread_id);

  // Terminates the usage of the map by freeing our resources.
  virtual HRESULT Terminate();

  // Return whether a tab ID is valid.
  //
  // @param tab_id The tab identifier.
  // @return True if the tab ID exists in the tab ID map.
  virtual bool IsTabIdValid(int tab_id);

  // Return a tab handle associated with the id.
  //
  // @param tab_id The tab identifier.
  // @return The corresponding HWND (or INVALID_HANDLE_VALUE if tab_id isn't
  //         found).
  virtual HWND GetTabHandleFromId(int tab_id);

  // Return whether a tab handle is valid.
  //
  // @param tab_handle The tab handle.
  // @return True if the tab handle exists in the tab handle map.
  virtual bool IsTabHandleValid(HWND tab_handle);

  // Return a tab ID associated with the HWND.
  //
  // @param tab_handle The tab HWND.
  // @return The corresponding tab ID (or 0 if tab_handle isn't found).
  virtual int GetTabIdFromHandle(HWND tab_handle);

  // Register the relation between a tab ID and an HWND.
  virtual void SetTabIdForHandle(int tab_id, HWND tab_handle);

  // Return a tab handle associated with the given tool band tab ID.
  //
  // @param tool_band_id The tab identifier for a tool band.
  // @return The corresponding HWND (or INVALID_HANDLE_VALUE if tool_band_id
  //         isn't found).
  virtual HWND GetTabHandleFromToolBandId(int tool_band_id);

  // Register the relation between a tool band tab ID and an HWND.
  virtual void SetTabToolBandIdForHandle(int tool_band_id, HWND tab_handle);

  // Unregister the HWND and its corresponding tab ID and tool band tab ID.
  virtual void DeleteTabHandle(HWND handle);

  // Cleans up the maps from all handles that would be associated to the given
  // thread id.
  virtual void CleanupMapsForThread(DWORD thread_id);

 protected:
  // Traits for Singleton<ExecutorsManager> so that we can pass an argument
  // to the constructor.
  struct SingletonTraits : public DefaultSingletonTraits<ExecutorsManager> {
    static ExecutorsManager* New() {
      if (test_instance_)
        return test_instance_;
      else
        return new ExecutorsManager(false);  // By default, we want a thread.
    }
  };

  // The data we pass to start our worker thread.
  // THERE IS A COPY OF THIS CLASS IN THE UNITTEST WHICH YOU NEED TO UPDATE IF
  // you change this one...
  struct ThreadStartData {
    ExecutorsManager* me;
    CHandle thread_started_gate;
  };

  // A structures holding on the info about an executor and thread it runs in.
  struct ExecutorInfo {
    ExecutorInfo(IUnknown* new_executor = NULL, HANDLE handle = NULL)
        : executor(new_executor), thread_handle(handle) {
    }
    ExecutorInfo(const ExecutorInfo& executor_info)
        : executor(executor_info.executor),
        thread_handle(executor_info.thread_handle) {
    }
    CComPtr<IUnknown> executor;
    // mutable so that we can assign/Detach a const copy.
    mutable CHandle thread_handle;
  };

  typedef std::map<ThreadId, ExecutorInfo> ExecutorsMap;
  typedef std::map<ThreadId, CHandle> Tid2Event;
  typedef std::map<int, HWND> TabIdMap;
  typedef std::map<HWND, int> HandleMap;

  // The index of the termination event in the array of handles we wait for.
  static const size_t kTerminationHandleIndexOffset;

  // The index of the update event in the array of handles we wait for.
  static const size_t kUpdateHandleIndexOffset;

  // The index of the last event in the array of handles we wait for.
  static const size_t kLastHandleIndexOffset;

  // The number of extra handles we used for the events described above.
  static const size_t kExtraHandles;

  // Protected constructor to ensure single instance and initialize some
  // members. Set no_thread for testing...
  explicit ExecutorsManager(bool no_thread);

  // Creates an executor creator in a virtual method so we can override it in
  // Our unit test code.
  //
  // @param executor_creator Where to return the executor creator.
  virtual HRESULT GetExecutorCreator(
      ICeeeExecutorCreator** executor_creator);

  // Returns a list of HANDLEs of threads for which we have an executor.
  //
  // @param thread_handles Where to return at most @p num_threads handles.
  // @param thread_ids Where to return at most @p num_threads ThreadIds.
  // @param num_threads How many handles can fit in @p thread_handles.
  // @return How many handles have been added in @p thread_handles and the same
  //         ThreadIds have been added to @p thread_ids.
  virtual size_t GetThreadHandles(CHandle thread_handles[],
                                  ThreadId thread_ids[], size_t num_threads);

  // A few seams so that we don't have to mock the kernel functions.
  virtual DWORD WaitForSingleObject(HANDLE wait_handle, DWORD timeout);
  virtual DWORD WaitForMultipleObjects(DWORD num_handles,
      const HANDLE* wait_handles, BOOL wait_all, DWORD timeout);

  // The thread procedure that we use to clean up dead threads from the map.
  //
  // @param thread_data A small structure containing this and an event to
  //  signal when the thread has finished initializing itself.
  static DWORD WINAPI ThreadProc(LPVOID thread_data);

  // The map of executor and their thread handle keyed by thread identifiers.
  // Thread protected by lock_.
  ExecutorsMap executors_;

  // We remember the thread identifiers for which we are pending a registration
  // so that we make sure that we only accept registration that we initiate.
  // Also, for each pending registration we must wait on a different event
  // per thread_id that we are waiting for the registration of.
  // Thread protected by ExecutorsManager::lock_.
  Tid2Event pending_registrations_;

  // The mapping between a tab ID and the HWND of the window holding the BHO.
  // In DEBUG, this mapping will grow over time since we don't remove it on
  // DeleteTabHandle. This is useful for debugging as we know if a mapping has
  // been deleted and is invalidly used.
  // Thread protected by ExecutorsManager::lock_.
  TabIdMap tab_id_map_;
  HandleMap handle_map_;

  // The mapping between a tool band's tab ID and the HWND of the window
  // holding the BHO.
  // Thread protected by ExecutorsManager::lock_.
  TabIdMap tool_band_id_map_;
  HandleMap tool_band_handle_map_;

  // Temporary to remember which Frame window we've heard of before.
  // TODO(mad@chromium.org): Find a better structure.
  typedef std::map<HWND, std::list<HWND>> FrameTabsMap;
  FrameTabsMap frame_window_families_;

  // The handle to the thread running ThreadProc.
  CHandle thread_;

  // Used to signal the thread to reload the list of thread handles.
  CHandle update_threads_list_gate_;

  // Used to signal the thread to terminate.
  CHandle termination_gate_;

  // To protect the access to the maps (ExecutorsManager::executors_ &
  // ExecutorsManager::pending_registrations_ & tab_id_map_/handle_map_).
  base::Lock lock_;

  // Test seam.
  WindowEventsFunnel windows_events_funnel_;
  virtual WindowEventsFunnel& windows_events_funnel() {
    return windows_events_funnel_;
  }

  // Test seam too.
  static ExecutorsManager* test_instance_;

  DISALLOW_COPY_AND_ASSIGN(ExecutorsManager);
};

#endif  // CEEE_IE_BROKER_EXECUTORS_MANAGER_H_
