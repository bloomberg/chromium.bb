// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_COPRESENCE_SOCKET_COPRESENCE_SOCKET_RESOURCES_H_
#define EXTENSIONS_BROWSER_API_COPRESENCE_SOCKET_COPRESENCE_SOCKET_RESOURCES_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api/api_resource_manager.h"

namespace copresence_sockets {
class CopresencePeer;
class CopresenceSocket;
}

namespace extensions {

class CopresencePeerResource : public ApiResource {
 public:
  CopresencePeerResource(const std::string& owner_extension_id,
                         scoped_ptr<copresence_sockets::CopresencePeer> peer);
  ~CopresencePeerResource() override;

  copresence_sockets::CopresencePeer* peer();

  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::UI;

 private:
  friend class ApiResourceManager<CopresencePeerResource>;
  static const char* service_name() { return "CopresencePeerResourceManager"; }

  scoped_ptr<copresence_sockets::CopresencePeer> peer_;

  DISALLOW_COPY_AND_ASSIGN(CopresencePeerResource);
};

class CopresenceSocketResource : public ApiResource {
 public:
  CopresenceSocketResource(
      const std::string& owner_extension_id,
      scoped_ptr<copresence_sockets::CopresenceSocket> socket);
  ~CopresenceSocketResource() override;

  copresence_sockets::CopresenceSocket* socket() { return socket_.get(); }

  void add_to_packet(const std::string& data) { packet_.append(data); }
  void clear_packet() { packet_.clear(); }
  const std::string& packet() { return packet_; }

  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::UI;

 private:
  friend class ApiResourceManager<CopresenceSocketResource>;
  static const char* service_name() {
    return "CopresenceSocketResourceManager";
  }

  scoped_ptr<copresence_sockets::CopresenceSocket> socket_;
  std::string packet_;

  DISALLOW_COPY_AND_ASSIGN(CopresenceSocketResource);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_COPRESENCE_SOCKET_COPRESENCE_SOCKET_RESOURCES_H_
