// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_CONTENT_HANDLER_CONNECTION_H_
#define MOJO_SHELL_CONTENT_HANDLER_CONNECTION_H_

#include <string>

#include "mojo/application/public/interfaces/content_handler.mojom.h"
#include "mojo/public/cpp/bindings/error_handler.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

class ApplicationManager;

// A ContentHandlerConnection is responsible for creating and maintaining a
// connection to an app which provides the ContentHandler service.
// A ContentHandlerConnection can only be destroyed via CloseConnection.
// A ContentHandlerConnection manages its own lifetime and cannot be used with
// a scoped_ptr to avoid reentrant calls into ApplicationManager late in
// destruction.
class ContentHandlerConnection : public ErrorHandler {
 public:
  ContentHandlerConnection(ApplicationManager* manager,
                           const GURL& content_handler_url,
                           const GURL& requestor_url,
                           const std::string& qualifier);

  // Closes the connection and destorys |this| object.
  void CloseConnection();

  ContentHandler* content_handler() { return content_handler_.get(); }
  const GURL& content_handler_url() { return content_handler_url_; }
  const std::string& content_handler_qualifier() {
    return content_handler_qualifier_;
  }

 private:
  ~ContentHandlerConnection() override;

  // ErrorHandler implementation:
  void OnConnectionError() override;

  ApplicationManager* manager_;
  GURL content_handler_url_;
  std::string content_handler_qualifier_;
  ContentHandlerPtr content_handler_;
  bool connection_closed_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerConnection);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_CONTENT_HANDLER_CONNECTION_H_
