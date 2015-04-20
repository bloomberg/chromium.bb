// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_HTTP_HANDLER_DEVTOOLS_HTTP_HANDLER_H_
#define COMPONENTS_DEVTOOLS_HTTP_HANDLER_DEVTOOLS_HTTP_HANDLER_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "net/http/http_status_code.h"

class GURL;

namespace base {
class Thread;
class Value;
}

namespace net {
class IPEndPoint;
class HttpServerRequestInfo;
class ServerSocket;
}

namespace devtools_http_handler {

class DevToolsAgentHostClientImpl;
class DevToolsHttpHandlerDelegate;
class ServerWrapper;

// This class is used for managing DevTools remote debugging server.
// Clients can connect to the specified ip:port and start debugging
// this browser.
class DevToolsHttpHandler {
 public:

  // Factory of net::ServerSocket. This is to separate instantiating dev tools
  // and instantiating server sockets.
  // All methods including destructor are called on a separate thread
  // different from any BrowserThread instance.
  class ServerSocketFactory {
   public:
    virtual ~ServerSocketFactory() {}

    // Returns a new instance of ServerSocket or nullptr if an error occurred.
    virtual scoped_ptr<net::ServerSocket> CreateForHttpServer();

    // Creates a named socket for reversed tethering implementation (used with
    // remote debugging, primarily for mobile).
    virtual scoped_ptr<net::ServerSocket> CreateForTethering(
        std::string* out_name);
  };

  // Takes ownership over |socket_factory| and |delegate|.
  // If |frontend_url| is empty, assumes it's bundled, and uses
  // |delegate->GetFrontendResource()|.
  // |delegate| is only accessed on UI thread.
  // If |active_port_output_directory| is non-empty, it is assumed the
  // socket_factory was initialized with an ephemeral port (0). The
  // port selected by the OS will be written to a well-known file in
  // the output directory.
  // TODO(dgozman): remove |manager_delegate| by moving targets to another
  // component (http://crbug.com/476496).
  DevToolsHttpHandler(
      scoped_ptr<ServerSocketFactory> server_socket_factory,
      const std::string& frontend_url,
      DevToolsHttpHandlerDelegate* delegate,
      content::DevToolsManagerDelegate* manager_delegate,
      const base::FilePath& active_port_output_directory,
      const base::FilePath& debug_frontend_dir,
      const std::string& product_name,
      const std::string& user_agent);
  ~DevToolsHttpHandler();

  // Returns the URL for the file at |path| in frontend.
  GURL GetFrontendURL(const std::string& path);

 private:
  friend class ServerWrapper;
  friend void ServerStartedOnUI(
      base::WeakPtr<DevToolsHttpHandler> handler,
      base::Thread* thread,
      ServerWrapper* server_wrapper,
      DevToolsHttpHandler::ServerSocketFactory* socket_factory,
      scoped_ptr<net::IPEndPoint> ip_address);

  void OnJsonRequest(int connection_id,
                     const net::HttpServerRequestInfo& info);
  void OnThumbnailRequest(int connection_id, const std::string& target_id);
  void OnDiscoveryPageRequest(int connection_id);
  void OnFrontendResourceRequest(int connection_id, const std::string& path);
  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info);
  void OnWebSocketMessage(int connection_id, const std::string& data);
  void OnClose(int connection_id);

  void ServerStarted(base::Thread* thread,
                     ServerWrapper* server_wrapper,
                     ServerSocketFactory* socket_factory,
                     scoped_ptr<net::IPEndPoint> ip_address);

  static void OnTargetListReceivedWeak(
      base::WeakPtr<DevToolsHttpHandler> handler,
      int connection_id,
      const std::string& host,
      const content::DevToolsManagerDelegate::TargetList& targets);
  void OnTargetListReceived(
      int connection_id,
      const std::string& host,
      const content::DevToolsManagerDelegate::TargetList& targets);

  content::DevToolsTarget* GetTarget(const std::string& id);

  void SendJson(int connection_id,
                net::HttpStatusCode status_code,
                base::Value* value,
                const std::string& message);
  void Send200(int connection_id,
               const std::string& data,
               const std::string& mime_type);
  void Send404(int connection_id);
  void Send500(int connection_id,
               const std::string& message);
  void AcceptWebSocket(int connection_id,
                       const net::HttpServerRequestInfo& request);

  // Returns the front end url without the host at the beginning.
  std::string GetFrontendURLInternal(const std::string target_id,
                                     const std::string& host);

  base::DictionaryValue* SerializeTarget(const content::DevToolsTarget& target,
                                         const std::string& host);

  // The thread used by the devtools handler to run server socket.
  base::Thread* thread_;
  std::string frontend_url_;
  std::string product_name_;
  std::string user_agent_;
  ServerWrapper* server_wrapper_;
  scoped_ptr<net::IPEndPoint> server_ip_address_;
  typedef std::map<int, DevToolsAgentHostClientImpl*> ConnectionToClientMap;
  ConnectionToClientMap connection_to_client_;
  const scoped_ptr<DevToolsHttpHandlerDelegate> delegate_;
  content::DevToolsManagerDelegate* manager_delegate_;
  ServerSocketFactory* socket_factory_;
  typedef std::map<std::string, content::DevToolsTarget*> TargetMap;
  TargetMap target_map_;
  base::WeakPtrFactory<DevToolsHttpHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsHttpHandler);
};

}  // namespace devtools_http_handler

#endif  // COMPONENTS_DEVTOOLS_HTTP_HANDLER_DEVTOOLS_HTTP_HANDLER_H_
