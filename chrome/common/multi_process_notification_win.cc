// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/multi_process_notification.h"

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

  bool Start();

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

bool ListenerImpl::Start() {
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

bool Listener::Start() {
  return impl_->Start();
}

}  // namespace multi_process_notification
