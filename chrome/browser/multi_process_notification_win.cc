// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multi_process_notification.h"

#include "base/logging.h"

namespace multi_process_notification {

bool Post(const std::string& name, Domain domain) {
  // TODO(dmaclach): Implement
  NOTIMPLEMENTED();
  return false;
}

class ListenerImpl {
 public:
  ListenerImpl(const std::string& name,
               Domain domain,
               Listener::Delegate* delegate);

  bool Start(MessageLoop* io_loop_to_listen_on);

  std::string name() const { return name_; }
  Domain domain() const { return domain_; }

 private:
  std::string name_;
  Domain domain_;
  Listener::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ListenerImpl);
};

ListenerImpl::ListenerImpl(const std::string& name,
                           Domain domain,
                           Listener::Delegate* delegate)
    : name_(name), domain_(domain), delegate_(delegate) {
}

bool ListenerImpl::Start(MessageLoop* io_loop_to_listen_on) {
  // TODO(dmaclach): Implement
  NOTIMPLEMENTED();
  return false;
}

Listener::Listener(const std::string& name,
                   Domain domain,
                   Listener::Delegate* delegate)
    : impl_(new ListenerImpl(name, domain, delegate)) {
}

Listener::~Listener() {
}

bool Listener::Start(MessageLoop* io_loop_to_listen_on) {
  return impl_->Start(io_loop_to_listen_on);
}

std::string Listener::name() const {
  return impl_->name();
}

Domain Listener::domain() const {
  return impl_->domain();
}

}  // namespace multi_process_notification
