// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_NET_SERVER_CONNECTION_MANAGER_H_
#define CHROME_BROWSER_SYNC_ENGINE_NET_SERVER_CONNECTION_MANAGER_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/sync/engine/net/http_return.h"
#include "chrome/browser/sync/syncable/syncable_id.h"
#include "chrome/browser/sync/util/event_sys.h"
#include "chrome/browser/sync/util/pthread_helpers.h"
#include "chrome/browser/sync/util/signin.h"
#include "chrome/browser/sync/util/sync_types.h"

namespace syncable {
class WriteTransaction;
class DirectoryManager;
}

namespace sync_pb {
class ClientToServerMessage;
};

struct RequestTimingInfo;

namespace browser_sync {

class ClientToServerMessage;

// How many connection errors are accepted before network handles are closed
// and reopened.
static const int32 kMaxConnectionErrorsBeforeReset = 10;

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
    // auth error has occured (i.e. a 401)
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

  // Identifies the type of failure, if any.
  ServerConnectionCode server_status;
};

inline bool IsGoodReplyFromServer(HttpResponse::ServerConnectionCode code) {
  return code >= HttpResponse::SERVER_CONNECTION_OK;
}

struct ServerConnectionEvent {
  enum { SHUTDOWN, STATUS_CHANGED } what_happened;
  HttpResponse::ServerConnectionCode connection_code;
  bool server_reachable;

  // Traits.
  typedef ServerConnectionEvent EventType;
  static inline bool IsChannelShutdownEvent(const EventType& event) {
    return SHUTDOWN == event.what_happened;
  }
};

struct WatchServerStatus;

// Use this class to interact with the sync server.
// The ServerConnectionManager currently supports POSTing protocol buffers.
//
//  *** This class is thread safe. In fact, you should consider creating only
//  one instance for every server that you need to talk to.
class ServerConnectionManager {
  friend class Post;
  friend struct WatchServerStatus;
 public:
  typedef EventChannel<ServerConnectionEvent, Lock> Channel;

  // The lifetime of the GaiaAuthenticator must be longer than the instance
  // of the ServerConnectionManager that you're creating.
  ServerConnectionManager(const std::string& server, int port, bool use_ssl,
                          const std::string& user_agent,
                          const std::string& client_id);

  virtual ~ServerConnectionManager();

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
    virtual bool Init(const char* path, const std::string& auth_token,
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
                                  const std::string& path, bool use_ssl) const;

    void GetServerParams(std::string* server, int* server_port,
                         bool* use_ssl) {
      ServerConnectionManager::ParametersLock lock(
          &scm_->server_parameters_mutex_);
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

  // POSTS buffer_in and reads a response into buffer_out. Uses our currently
  // set auth token in our headers.
  //
  // Returns true if executed successfully.
  virtual bool PostBufferWithCachedAuth(const PostBufferParams* params);

  // POSTS buffer_in and reads a response into buffer_out. Add a specific auth
  // token to http headers.
  //
  // Returns true if executed successfully.
  virtual bool PostBufferWithAuth(const PostBufferParams* params,
                                  const std::string& auth_token);

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

  inline std::string user_agent() const { return user_agent_; }

  inline HttpResponse::ServerConnectionCode server_status() const {
    return server_status_;
  }

  inline bool server_reachable() const { return server_reachable_; }

  void ResetAuthStatus();

  void ResetConnection();

  void NotifyStatusChanged();

  const std::string client_id() const { return client_id_; }

  void SetDomainFromSignIn(SignIn signin_type, const std::string& signin);

  // This changes the server info used by the connection manager. This allows
  // a single client instance to talk to different backing servers. This is
  // typically called during / after authentication so that the server url
  // can be a function of the user's login id. A side effect of this call is
  // that ResetConnection is called.
  void SetServerParameters(const std::string& server_url, int port,
                           bool use_ssl);

  // Returns the current server parameters in server_url, port and use_ssl.
  void GetServerParameters(std::string* server_url, int* port, bool* use_ssl);

  bool terminate_all_io() const {
    PThreadScopedLock<PThreadMutex> lock(&terminate_all_io_mutex_);
    return terminate_all_io_;
  }

  // Factory method to create a Post object we can use for communication with
  // the server.
  virtual Post* MakePost() {
    return NULL;  // For testing.
  };

  void set_auth_token(const std::string& auth_token) {
    auth_token_.assign(auth_token);
  }

 protected:

  PThreadMutex shutdown_event_mutex_;
  PThreadCondVar shutdown_event_condition_;

  // Protects access to sync_server_, sync_server_port_ and use_ssl_:
  mutable PThreadMutex server_parameters_mutex_;
  typedef PThreadScopedLock<PThreadMutex> ParametersLock;

  // The sync_server_ is the server that requests will be made to.
  std::string sync_server_;

  // The sync_server_port_ is the port that HTTP requests will be made on.
  int sync_server_port_;

  // The unique id of the user's client.
  const std::string client_id_;

  // The user-agent string for HTTP.
  std::string user_agent_;

  // Indicates whether or not requests should be made using HTTPS.
  bool use_ssl_;

  // The paths we post to.
  mutable PThreadMutex path_mutex_;
  typedef PThreadScopedLock<PThreadMutex> ScopedPathLock;

  std::string proto_sync_path_;
  std::string get_time_path_;

  // The auth token to use in authenticated requests. Set by the AuthWatcher.
  std::string auth_token_;

  inline std::string proto_sync_path() const {
    ScopedPathLock lock(&path_mutex_);
    return proto_sync_path_;
  }
  std::string get_time_path() const {
    ScopedPathLock lock(&path_mutex_);
    return get_time_path_;
  }

  // Called wherever a failure should be taken as an indication that we may
  // be experiencing connection difficulties.
  virtual bool IncrementErrorCount();
  mutable PThreadMutex error_count_mutex_;  // Protects error_count_
  int error_count_;  // Tracks the number of connection errors.

 protected:
  Channel* const channel_;
  // Volatile so various threads can call server_status() without
  // synchronization.
  volatile HttpResponse::ServerConnectionCode server_status_;
  bool server_reachable_;

  struct PlatformMembers;  // Contains platform specific member vars.
  PlatformMembers* const platform_;

  // A counter that is incremented everytime ResetAuthStatus() is called.
  volatile base::subtle::AtomicWord reset_count_;

  // NOTE: Tests rely on this protected function being virtual.
  //
  // Internal PostBuffer base function.
  virtual bool PostBufferToPath(const PostBufferParams*,
                                const std::string& path,
                                const std::string& auth_token);

 private:
  mutable PThreadMutex terminate_all_io_mutex_;
  bool terminate_all_io_;  // When set to true, terminate all connections asap.
  DISALLOW_COPY_AND_ASSIGN(ServerConnectionManager);
};

// Fills a ClientToServerMessage with the appropriate share and birthday
// settings.
bool FillMessageWithShareDetails(sync_pb::ClientToServerMessage* csm,
                                 syncable::DirectoryManager* manager,
                                 const PathString &share);

}  // namespace browser_sync

std::ostream& operator<<(std::ostream& s,
    const struct browser_sync::HttpResponse& hr);

#endif  // CHROME_BROWSER_SYNC_ENGINE_NET_SERVER_CONNECTION_MANAGER_H_
