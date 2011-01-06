// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/multi_process_notification.h"

#import <Foundation/Foundation.h>
#include <notify.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop.h"
#include "base/message_pump_libevent.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "base/task.h"
#include "chrome/common/chrome_paths.h"

namespace {

std::string AddPrefixToNotification(const std::string& name,
                                    multi_process_notification::Domain domain) {
  // The ordering of the components in the string returned by this function
  // is important. Read "NAMESPACE CONVENTIONS" in 'man 3 notify' for details.
  base::mac::ScopedNSAutoreleasePool pool;
  NSBundle *bundle = base::mac::MainAppBundle();
  NSString *ns_bundle_id = [bundle bundleIdentifier];
  std::string bundle_id = base::SysNSStringToUTF8(ns_bundle_id);
  std::string domain_string;
  switch (domain) {
    case multi_process_notification::ProfileDomain: {
      FilePath user_data_dir;
      if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
        NOTREACHED();
      }
      domain_string = StringPrintf("user.uid.%u.%s.",
                                   getuid(), user_data_dir.value().c_str());
      break;
    }

    case multi_process_notification::UserDomain:
      domain_string = StringPrintf("user.uid.%u.", getuid());
      break;

    case multi_process_notification::SystemDomain:
      break;
  }
  return domain_string + bundle_id + "." + name;
}


}  // namespace

namespace multi_process_notification {

bool Post(const std::string& name, Domain domain) {
  std::string notification = AddPrefixToNotification(name, domain);
  uint32_t status = notify_post(notification.c_str());
  DCHECK_EQ(status, static_cast<uint32_t>(NOTIFY_STATUS_OK));
  return status == NOTIFY_STATUS_OK;
}


class ListenerImpl : public base::MessagePumpLibevent::Watcher {
 public:
  ListenerImpl(const std::string& name,
               Domain domain,
               Listener::Delegate* delegate);
  virtual ~ListenerImpl();

  bool Start();

  virtual void OnFileCanReadWithoutBlocking(int fd);
  virtual void OnFileCanWriteWithoutBlocking(int fd);

 private:
  std::string name_;
  Domain domain_;
  Listener::Delegate* delegate_;
  int fd_;
  int token_;
  base::MessagePumpLibevent::FileDescriptorWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(ListenerImpl);
};

ListenerImpl::ListenerImpl(const std::string& name,
                           Domain domain,
                           Listener::Delegate* delegate)
    : name_(name), domain_(domain), delegate_(delegate), fd_(-1), token_(-1) {
}

ListenerImpl::~ListenerImpl() {
  if (token_ != -1) {
    uint32_t status = notify_cancel(token_);
    DCHECK_EQ(status, static_cast<uint32_t>(NOTIFY_STATUS_OK));
    token_ = -1;
  }
}

bool ListenerImpl::Start() {
  DCHECK_EQ(fd_, -1);
  DCHECK_EQ(token_, -1);
  std::string notification = AddPrefixToNotification(name_, domain_);

  uint32_t status = notify_register_file_descriptor(notification.c_str(), &fd_,
                                                    0, &token_);
  if (status != NOTIFY_STATUS_OK) {
    LOG(ERROR) << "Unable to notify_register_file_descriptor for '"
               << notification << "' Status: " << status;
    return false;
  }

  MessageLoopForIO *io_loop = MessageLoopForIO::current();
  return io_loop->WatchFileDescriptor(fd_, true, MessageLoopForIO::WATCH_READ,
                                      &watcher_, this);
}

void ListenerImpl::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(fd, fd_);
  int token = 0;
  if (HANDLE_EINTR(read(fd_, &token, sizeof(token))) >= 0) {
    // Have to swap to native endianness <http://openradar.appspot.com/8821081>.
    token = static_cast<int>(ntohl(token));
    if (token == token_) {
      delegate_->OnNotificationReceived(name_, domain_);
    } else {
      LOG(WARNING) << "Unexpected value " << token;
    }
  }
}

void ListenerImpl::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
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
