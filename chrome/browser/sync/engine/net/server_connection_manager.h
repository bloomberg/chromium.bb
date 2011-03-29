// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_NET_SERVER_CONNECTION_MANAGER_H_
#define CHROME_BROWSER_SYNC_ENGINE_NET_SERVER_CONNECTION_MANAGER_H_
#pragma once

#include <iosfwd>
#include <string>

#include "base/atomicops.h"
#include "base/observer_list_threadsafe.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/sync/syncable/syncable_id.h"
#include "chrome/common/deprecated/event_sys.h"
#include "chrome/common/deprecated/event_sys-inl.h"
#include "chrome/common/net/http_return.h"

namespace syncable {
class WriteTransaction;
class DirectoryManager;
}

namespace sync_pb {
class ClientToServerMessage;
}

struct RequestTimingInfo;

namespace browser_sync {

class ClientToServerMessage;

// How many connection errors are accepted before network handles are closed
// and reopened.
static const int32 kMaxConnectionErrorsBeforeReset = 10;

static const int32 kUnsetResponseCode = -1;
static const int32 kUnsetContentLength = -1;
static const int32 kUnsetPayloadLength = -1;

// HttpResponse gathers the relevant output properties of an HTTP request.
// Depending on the value of the server_status code, response_code, and
// content_length may not be valid.
struct HttpResponse {
  enum ServerConnectionCode {
    // For uninitialized state.
    NONE,

    // CONNECTION_UNAVAILABLE is returned when InternetConnect() fails.
    CONNECTION_UNAVAILABLE,

    // IO_ERROR is returned when reading/writing to a buffer has failed.
    IO_ERROR,

    // SYNC_SERVER_ERROR is returned when the HTTP status code indicates that
    // a non-auth error has occured.
    SYNC_SERVER_ERROR,

    // SYNC_AUTH_ERROR is returned when the HTTP status code indicates that an
    // auth error has occured (i.e. a 401 or sync-specific AUTH_INVALID
    // response)
    // TODO(tim): Caring about AUTH_INVALID is a layering violation. But
    // this app-specific logic is being added as a stable branch hotfix so
    // minimal changes prevail for the moment.  Fix this! Bug 35060.
    SYNC_AUTH_ERROR,

    // All the following connection codes are valid responses from the server.
    // Means the server is up.  If you update this list, be sure to also update
    // IsGoodReplyFromServer().

    // SERVER_CONNECTION_OK is returned when request was handled correctly.
    SERVER_CONNECTION_OK,

    // RETRY is returned when a Commit request fails with a RETRY response from
    // the server.
    //
    // TODO(idana): the server no longer returns RETRY so we should remove this
    // value.
    RETRY,
  };

  // The HTTP Status code.
  int64 response_code;

  // The value of the Content-length header.
  int64 content_length;

  // The size of a download request's payload.
  int64 payload_length;

  // Value of the Update-Client-Auth header.
  std::string update_client_auth_header;

  // Identifies the type of failure, if any.
  ServerConnectionCode server_status;

  HttpResponse()
      : response_code(kUnsetResponseCode),
        content_length(kUnsetContentLength),
        payload_length(kUnsetPayloadLength),
        server_status(NONE) {}
};

inline bool IsGoodReplyFromServer(HttpResponse::ServerConnectionCode code) {
  return code >= HttpResponse::SERVER_CONNECTION_OK;
}

// TODO(tim): Deprecated.
struct ServerConnectionEvent {
  // Traits.
  typedef ServerConnectionEvent EventType;
  enum WhatHappened {
    SHUTDOWN,
    STATUS_CHANGED
  };

  static inline bool IsChannelShutdownEvent(const EventType& event) {
    return SHUTDOWN == event.what_happened;
  }

  WhatHappened what_happened;
  HttpResponse::ServerConnectionCode connection_code;
  bool server_reachable;
};

struct ServerConnectionEvent2 {
  HttpResponse::ServerConnectionCode connection_code;
  bool server_reachable;
  ServerConnectionEvent2(HttpResponse::ServerConnectionCode code,
                         bool server_reachable) :
      connection_code(code), server_reachable(server_reachable) {}
};

class ServerConnectionEventListener {
 public:
  virtual void OnServerConnectionEvent(const ServerConnectionEvent2& event) = 0;
 protected:
  virtual ~ServerConnectionEventListener() {}
};

class ServerConnectionManager;
// A helper class that automatically notifies when the status changes.
// TODO(tim): This class shouldn't be exposed outside of the implementation,
// bug 35060.
class ScopedServerStatusWatcher {
 public:
  ScopedServerStatusWatcher(ServerConnectionManager* conn_mgr,
                            HttpResponse* response);
  ~ScopedServerStatusWatcher();
 private:
  ServerConnectionManager* const conn_mgr_;
  HttpResponse* const response_;
  // TODO(tim): Should this be Barrier:AtomicIncrement?
  base::subtle::AtomicWord reset_count_;
  bool server_reachable_;
  DISALLOW_COPY_AND_ASSIGN(ScopedServerStatusWatcher);
};

// Use this class to interact with the sync server.
// The ServerConnectionManager currently supports POSTing protocol buffers.
//
//  *** This class is thread safe. In fact, you should consider creating only
//  one instance for every server that you need to talk to.
class ServerConnectionManager {
 public:
  typedef EventChannel<ServerConnectionEvent, base::Lock> Channel;

  // buffer_in - will be POSTed
  // buffer_out - string will be overwritten with response
  struct PostBufferParams {
    const std::string& buffer_in;
    std::string* buffer_out;
    HttpResponse* response;
    RequestTimingInfo* timing_info;
  };

  // Abstract class providing network-layer functionality to the
  // ServerConnectionManager. Subclasses implement this using an HTTP stack of
  // their choice.
  class Post {
   public:
    explicit Post(ServerConnectionManager* scm) : scm_(scm), timing_info_(0) {
    }
    virtual ~Post() { }

    // Called to initialize and perform an HTTP POST.
    virtual bool Init(const char* path,
                      const std::string& auth_token,
                      const std::string& payload,
                      HttpResponse* response) = 0;

    bool ReadBufferResponse(std::string* buffer_out, HttpResponse* response,
                            bool require_response);
    bool ReadDownloadResponse(HttpResponse* response, std::string* buffer_out);

    void set_timing_info(RequestTimingInfo* timing_info) {
      timing_info_ = timing_info;
    }
    RequestTimingInfo* timing_info() { return timing_info_; }

   protected:
    std::string MakeConnectionURL(const std::string& sync_server,
                                  const std::string& path,
                                  bool use_ssl) const;

    void GetServerParams(std::string* server,
                         int* server_port,
                         bool* use_ssl) const {
      base::AutoLock lock(scm_->server_parameters_mutex_);
      server->assign(scm_->sync_server_);
      *server_port = scm_->sync_server_port_;
      *use_ssl = scm_->use_ssl_;
    }

    std::string buffer_;
    ServerConnectionManager* scm_;

   private:
    int ReadResponse(void* buffer, int length);
    int ReadResponse(std::string* buffer, int length);
    RequestTimingInfo* timing_info_;
  };

  ServerConnectionManager(const std::string& server,
                          int port,
                          bool use_ssl,
                          const std::string& user_agent);

  virtual ~ServerConnectionManager();

  // POSTS buffer_in and reads a response into buffer_out. Uses our currently
  // set auth token in our headers.
  //
  // Returns true if executed successfully.
  virtual bool PostBufferWithCachedAuth(const PostBufferParams* params,
                                        ScopedServerStatusWatcher* watcher);

  // Checks the time on the server. Returns false if the request failed. |time|
  // is an out parameter that stores the value returned from the server.
  virtual bool CheckTime(int32* out_time);

  // Returns true if sync_server_ is reachable. This method verifies that the
  // server is pingable and that traffic can be sent to and from it.
  virtual bool IsServerReachable();

  // Returns true if user has been successfully authenticated.
  virtual bool IsUserAuthenticated();

  // Updates status and broadcasts events on change.
  bool CheckServerReachable();

  // Signal the shutdown event to notify listeners.
  virtual void kill();

  inline Channel* channel() const { return channel_; }

  void AddListener(ServerConnectionEventListener* listener);
  void RemoveListener(ServerConnectionEventListener* listener);

  inline std::string user_agent() const { return user_agent_; }

  inline HttpResponse::ServerConnectionCode server_status() const {
    return server_status_;
  }

  inline bool server_reachable() const { return server_reachable_; }

  const std::string client_id() const { return client_id_; }

  // This changes the server info used by the connection manager. This allows
  // a single client instance to talk to different backing servers. This is
  // typically called during / after authentication so that the server url
  // can be a function of the user's login id. A side effect of this call is
  // that ResetConnection is called.
  void SetServerParameters(const std::string& server_url,
                           int port,
                           bool use_ssl);

  // Returns the current server parameters in server_url, port and use_ssl.
  void GetServerParameters(std::string* server_url,
                           int* port,
                           bool* use_ssl) const;

  std::string GetServerHost() const;

  bool terminate_all_io() const {
    base::AutoLock lock(terminate_all_io_mutex_);
    return terminate_all_io_;
  }

  // Factory method to create a Post object we can use for communication with
  // the server.
  virtual Post* MakePost();

  void set_client_id(const std::string& client_id) {
    DCHECK(client_id_.empty());
    client_id_.assign(client_id);
  }

  void set_auth_token(const std::string& auth_token) {
    // TODO(chron): Consider adding a message loop check here.
    base::AutoLock lock(auth_token_mutex_);
    auth_token_.assign(auth_token);
  }

  const std::string auth_token() const {
    base::AutoLock lock(auth_token_mutex_);
    return auth_token_;
  }

 protected:
  inline std::string proto_sync_path() const {
    base::AutoLock lock(path_mutex_);
    return proto_sync_path_;
  }

  std::string get_time_path() const {
    base::AutoLock lock(path_mutex_);
    return get_time_path_;
  }

  // Called wherever a failure should be taken as an indication that we may
  // be experiencing connection difficulties.
  virtual bool IncrementErrorCount();

  // NOTE: Tests rely on this protected function being virtual.
  //
  // Internal PostBuffer base function.
  virtual bool PostBufferToPath(const PostBufferParams*,
                                const std::string& path,
                                const std::string& auth_token,
                                ScopedServerStatusWatcher* watcher);

  // Protects access to sync_server_, sync_server_port_ and use_ssl_:
  mutable base::Lock server_parameters_mutex_;

  // The sync_server_ is the server that requests will be made to.
  std::string sync_server_;

  // The sync_server_port_ is the port that HTTP requests will be made on.
  int sync_server_port_;

  // The unique id of the user's client.
  std::string client_id_;

  // The user-agent string for HTTP.
  std::string user_agent_;

  // Indicates whether or not requests should be made using HTTPS.
  bool use_ssl_;

  // The paths we post to.
  mutable base::Lock path_mutex_;
  std::string proto_sync_path_;
  std::string get_time_path_;

  mutable base::Lock auth_token_mutex_;
  // The auth token to use in authenticated requests. Set by the AuthWatcher.
  std::string auth_token_;

  base::Lock error_count_mutex_;  // Protects error_count_
  int error_count_;  // Tracks the number of connection errors.

  // TODO(tim): Deprecated.
  Channel* const channel_;

  scoped_refptr<ObserverListThreadSafe<ServerConnectionEventListener> >
     listeners_;

  // Volatile so various threads can call server_status() without
  // synchronization.
  volatile HttpResponse::ServerConnectionCode server_status_;
  bool server_reachable_;

  // A counter that is incremented everytime ResetAuthStatus() is called.
  volatile base::subtle::AtomicWord reset_count_;

 private:
  friend class Post;
  friend class ScopedServerStatusWatcher;

  void NotifyStatusChanged();
  void ResetConnection();

  mutable base::Lock terminate_all_io_mutex_;
  bool terminate_all_io_;  // When set to true, terminate all connections asap.
  DISALLOW_COPY_AND_ASSIGN(ServerConnectionManager);
};

// Fills a ClientToServerMessage with the appropriate share and birthday
// settings.
bool FillMessageWithShareDetails(sync_pb::ClientToServerMessage* csm,
                                 syncable::DirectoryManager* manager,
                                 const std::string& share);

std::ostream& operator<<(std::ostream& s, const struct HttpResponse& hr);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_NET_SERVER_CONNECTION_MANAGER_H_
