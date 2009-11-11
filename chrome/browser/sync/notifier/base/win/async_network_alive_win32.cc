// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/base/async_network_alive.h"

#include <winsock2.h>

#include "base/scoped_handle_win.h"
#include "chrome/browser/sync/notifier/base/utils.h"
#include "talk/base/common.h"
#include "talk/base/criticalsection.h"
#include "talk/base/logging.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/scoped_ptr.h"

namespace notifier {

class PlatformNetworkInfo {
 public:
  PlatformNetworkInfo() : ws_handle_(NULL), event_handle_(NULL) {
  }

  ~PlatformNetworkInfo() {
    Close();
  }

  void Close() {
    talk_base::CritScope crit_scope(&crit_sect_);
    if (ws_handle_) {
      if (event_handle_.IsValid())  // Unblock any waiting for network changes.
        SetEvent(event_handle_.Get());
      // finishes the iteration.
      VERIFY(WSALookupServiceEnd(ws_handle_) == 0);
      ws_handle_ = NULL;
      LOG(INFO) << "WSACleanup 1";
      ::WSACleanup();
    }
  }

  bool IsAlive(bool* error) {
    ASSERT(error);
    *error = false;

    // If IsAlive was previously called, we need a new handle.
    // Why? If we use the same handle, we only get diffs on what changed
    // which isn't what we want.
    Close();
    int result = Initialize();
    if (result != 0) {
      LOG(ERROR) << "failed:" << result;
      // Default to alive on error.
      *error = true;
      return true;
    }

    bool alive = false;

    // Retrieve network info and move to next one. In this function, we only
    // need to know whether or not there is network connection.
    // allocate 256 bytes for name, it should be enough for most cases.
    // If the name is longer, it is OK as we will check the code returned and
    // set correct network status.
    char result_buffer[sizeof(WSAQUERYSET) + 256] = {0};
    bool flush_previous_result = false;
    do {
      DWORD control_flags = LUP_RETURN_NAME;
      if (flush_previous_result) {
        control_flags |= LUP_FLUSHPREVIOUS;
      }
      DWORD length = sizeof(result_buffer);
      reinterpret_cast<WSAQUERYSET*>(&result_buffer[0])->dwSize =
          sizeof(WSAQUERYSET);
      // ws_handle_ may be NULL (if exiting), but the call will simply fail
      int result = ::WSALookupServiceNext(
          ws_handle_,
          control_flags,
          &length,
          reinterpret_cast<WSAQUERYSET*>(&result_buffer[0]));

      if (result == 0) {
        // get at least one connection, return "connected".
        alive = true;
      } else {
        ASSERT(result == SOCKET_ERROR);
        result = ::WSAGetLastError();
        if (result == WSA_E_NO_MORE || result == WSAENOMORE) {
          break;
        }

        // Error code WSAEFAULT means there is a network connection but the
        // result_buffer size is too small to contain the results. The
        // variable "length" returned from WSALookupServiceNext is the minimum
        // number of bytes required. We do not need to retrieve detail info.
        // Return "alive" in this case.
        if (result == WSAEFAULT) {
          alive = true;
          flush_previous_result = true;
        } else {
          LOG(WARNING) << "failed:" << result;
          *error = true;
          break;
        }
      }
    } while (true);

    LOG(INFO) << "alive: " << alive;
    return alive;
  }

  bool WaitForChange() {
    //  IsAlive must be called first.
    int junk1 = 0, junk2 = 0;
    DWORD bytes_returned = 0;
    int result = SOCKET_ERROR;
    {
      talk_base::CritScope crit_scope(&crit_sect_);
      if (!ws_handle_)
        return false;
      ASSERT(!event_handle_.IsValid());
      event_handle_.Set(CreateEvent(NULL, FALSE, FALSE, NULL));
      if (!event_handle_.IsValid()) {
        LOG(WARNING) << "failed to CreateEvent";
        return false;
      }
      WSAOVERLAPPED overlapped = {0};
      overlapped.hEvent = event_handle_.Get();
      WSACOMPLETION completion;
      ::SetZero(completion);
      completion.Type = NSP_NOTIFY_EVENT;
      completion.Parameters.Event.lpOverlapped = &overlapped;

      LOG(INFO) << "calling WSANSPIoctl";
      // Do a non-blocking request for change notification.  event_handle_
      // will get signaled when there is a change, so we wait on it later.
      // It can also be signaled by Close() in order allow clean termination.
      result = ::WSANSPIoctl(ws_handle_,
                             SIO_NSP_NOTIFY_CHANGE,
                             &junk1,
                             0,
                             &junk2,
                             0,
                             &bytes_returned,
                             &completion);
    }
    if (NO_ERROR != result) {
      result = ::WSAGetLastError();
      if (WSA_IO_PENDING != result) {
        LOG(WARNING) << "failed: " << result;
        event_handle_.Close();
        return false;
      }
    }
    LOG(INFO) << "waiting";
    WaitForSingleObject(event_handle_.Get(), INFINITE);
    event_handle_.Close();
    LOG(INFO) << "changed";
    return true;
  }

 private:
  int Initialize() {
    WSADATA wsa_data;
    LOG(INFO) << "calling WSAStartup";
    int result = ::WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != ERROR_SUCCESS) {
      LOG(ERROR) << "failed:" << result;
      return result;
    }

    WSAQUERYSET query_set = {0};
    query_set.dwSize = sizeof(WSAQUERYSET);
    query_set.dwNameSpace = NS_NLA;
    // Initiate a client query to iterate through the
    // currently connected networks.
    if (0 != ::WSALookupServiceBegin(&query_set, LUP_RETURN_ALL,
                                     &ws_handle_)) {
      result = ::WSAGetLastError();
      LOG(INFO) << "WSACleanup 2";
      ::WSACleanup();
      ASSERT(ws_handle_ == NULL);
      ws_handle_ = NULL;
      return result;
    }
    return 0;
  }
  talk_base::CriticalSection crit_sect_;
  HANDLE ws_handle_;
  ScopedHandle event_handle_;
  DISALLOW_COPY_AND_ASSIGN(PlatformNetworkInfo);
};

class AsyncNetworkAliveWin32 : public AsyncNetworkAlive {
 public:
  AsyncNetworkAliveWin32() {
  }

  virtual ~AsyncNetworkAliveWin32() {
    if (network_info_) {
      delete network_info_;
      network_info_ = NULL;
    }
  }

 protected:
  // SignalThread Interface
  virtual void DoWork() {
    if (!network_info_) {
      network_info_ = new PlatformNetworkInfo();
    } else {
      // Since network_info is set, it means that
      // we are suppose to wait for network state changes.
      if (!network_info_->WaitForChange()) {
        // The wait was aborted so we must be shutting down.
        alive_ = false;
        error_ = true;
        return;
      }
    }

    if (network_info_->IsAlive(&error_)) {
      // If there is an active connection, check that www.google.com:80
      // is reachable.
      talk_base::PhysicalSocketServer physical;
      scoped_ptr<talk_base::Socket> socket(physical.CreateSocket(SOCK_STREAM));
      if (socket->Connect(talk_base::SocketAddress("talk.google.com", 5222))) {
        alive_ = false;
      } else {
        alive_ = true;
      }
    } else {
      // If there are no available connections, then we aren't alive.
      alive_ = false;
    }
  }

  virtual void OnWorkStop() {
    if (network_info_) {
      network_info_->Close();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AsyncNetworkAliveWin32);
};

AsyncNetworkAlive* AsyncNetworkAlive::Create() {
  return new AsyncNetworkAliveWin32();
}

}  // namespace notifier
