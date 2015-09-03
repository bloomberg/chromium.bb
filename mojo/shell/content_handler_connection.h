// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_CONTENT_HANDLER_CONNECTION_H_
#define MOJO_SHELL_CONTENT_HANDLER_CONNECTION_H_

#include <string>

#include "mojo/application/public/interfaces/content_handler.mojom.h"
#include "mojo/shell/capability_filter.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

class ApplicationManager;
struct Identity;

// A ContentHandlerConnection is responsible for creating and maintaining a
// connection to an app which provides the ContentHandler service.
// A ContentHandlerConnection can only be destroyed via CloseConnection.
// A ContentHandlerConnection manages its own lifetime and cannot be used with
// a scoped_ptr to avoid reentrant calls into ApplicationManager late in
// destruction.
class ContentHandlerConnection {
 public:
  // |id| is a unique identifier for this content handler.
  ContentHandlerConnection(ApplicationManager* manager,
                           const Identity& originator_identity,
                           const CapabilityFilter& originator_filter,
                           const GURL& content_handler_url,
                           const std::string& qualifier,
                           const CapabilityFilter& filter,
                           uint32_t id);

  // Closes the connection and destroys |this| object.
  void CloseConnection();

  ContentHandler* content_handler() { return content_handler_.get(); }
  const GURL& content_handler_url() { return content_handler_url_; }
  const std::string& content_handler_qualifier() {
    return content_handler_qualifier_;
  }
  uint32_t id() const { return id_; }

 private:
  ~ContentHandlerConnection();

  ApplicationManager* manager_;
  GURL content_handler_url_;
  std::string content_handler_qualifier_;
  ContentHandlerPtr content_handler_;
  bool connection_closed_;
  // The id for this content handler.
  const uint32_t id_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerConnection);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_CONTENT_HANDLER_CONNECTION_H_
