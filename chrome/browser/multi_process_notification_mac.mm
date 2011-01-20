// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multi_process_notification.h"

#import <Foundation/Foundation.h>
#include <notify.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>

#include "base/basictypes.h"
#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop_proxy.h"
#include "base/message_pump_libevent.h"
#include "base/path_service.h"
#include "base/ref_counted.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/sys_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/simple_thread.h"
#include "chrome/common/chrome_paths.h"

// Enable this to build with leopard_switchboard_thread
// #define USE_LEOPARD_SWITCHBOARD_THREAD 1

namespace {

std::string AddPrefixToNotification(const std::string& name,
                                    multi_process_notification::Domain domain) {
  // The ordering of the components in the string returned by this function
  // is important. Read "NAMESPACE CONVENTIONS" in 'man 3 notify' for details.
  base::mac::ScopedNSAutoreleasePool pool;
  NSBundle* bundle = base::mac::MainAppBundle();
  NSString* ns_bundle_id = [bundle bundleIdentifier];
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

bool UseLeopardSwitchboardThread() {
#if USE_LEOPARD_SWITCHBOARD_THREAD
  return true;
#endif  // USE_LEOPARD_SWITCHBOARD_THREAD
  int32 major_version, minor_version, bugfix_version;
  base::SysInfo::OperatingSystemVersionNumbers(
      &major_version, &minor_version, &bugfix_version);
  return major_version < 10 || (major_version == 10 && minor_version <= 5);
}

}  // namespace

namespace multi_process_notification {

bool Post(const std::string& name, Domain domain) {
  std::string notification = AddPrefixToNotification(name, domain);
  uint32_t status = notify_post(notification.c_str());
  DCHECK_EQ(status, static_cast<uint32_t>(NOTIFY_STATUS_OK));
  return status == NOTIFY_STATUS_OK;
}

#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
#error LeopardSwitchboardThread can be removed
#endif  // MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5

// LeopardSwitchboardThread exists because the file descriptors returned by
// notify_register_file_descriptor can't be monitored using kqueues on 10.5
// ( http://openradar.appspot.com/8854692 ) and libevent uses kqueue to watch
// file descriptors in IOMessageLoop.
// This solution is to have a separate thread that monitors the file descriptor
// returned by notify_register_file_descriptor using select, and then to
// notify the MessageLoopForIO using a different file descriptor allocated by
// socketpair that can be monitored using kqueues in libevent. This thread
// only runs on 10.5, as 10.6 kqueues can monitor the notify file descriptors
// without any problems.

// LeopardSwitchboardThread creates three file descriptors:
// internal_fd_: which communicates from the switchboard thread to other threads
// external_fd_: which communicates from other threads to the switchboard thread
// notify_fd_: which is the file descriptor returned from
//             notify_register_file_descriptor
//
// The thread itself sits in a select loop waiting on internal_fd_, and
// notify_fd_ for input. If it gets ANY input on internal_fd_ it exits.
// If it gets input on notify_fd_ it sends the input through to external_fd_.
// External_fd_ is monitored by MessageLoopForIO so that the lookup of any
// matching listeners in entries_, and the triggering of those listeners,
// occurs in the MessageLoopForIO thread.
//
// Lookups are linear right now, and could be optimized if they ever become
// a performance issue.
class LeopardSwitchboardThread
    : public base::MessagePumpLibevent::Watcher,
      public base::SimpleThread,
      public MessageLoop::DestructionObserver {
 public:
  LeopardSwitchboardThread();
  virtual ~LeopardSwitchboardThread();

  bool Init();

  bool AddListener(ListenerImpl* listener,
                   const std::string& notification);
  bool RemoveListener(ListenerImpl* listener, const std::string& notification);

  bool finished() const { return finished_; }

  // SimpleThread overrides
  virtual void Run();

  // Watcher overrides
  virtual void OnFileCanReadWithoutBlocking(int fd);
  virtual void OnFileCanWriteWithoutBlocking(int fd);

  // DestructionObserver overrides
  virtual void WillDestroyCurrentMessageLoop();

 private:
  // Used to match tokens to notifications and vice-versa.
  struct SwitchboardEntry {
    int token_;
    std::string notification_;
    ListenerImpl* listener_;
  };

  enum {
    kKillThreadMessage = 0xdecea5e
  };

  int internal_fd_;
  int external_fd_;
  int notify_fd_;
  int notify_fd_token_;
  mutable bool finished_;
  fd_set fd_set_;

  // all accesses to entries_ must be controlled by entries_lock_.
  std::vector<SwitchboardEntry> entries_;
  Lock entries_lock_;
  base::MessagePumpLibevent::FileDescriptorWatcher watcher_;
};

class ListenerImpl : public base::MessagePumpLibevent::Watcher {
 public:
  ListenerImpl(const std::string& name,
               Domain domain,
               Listener::Delegate* delegate);
  virtual ~ListenerImpl();

  bool Start(MessageLoop* io_loop_to_listen_on);

  std::string name() const { return name_; }
  Domain domain() const { return domain_; }

  void OnListen();

  // Watcher overrides
  virtual void OnFileCanReadWithoutBlocking(int fd);
  virtual void OnFileCanWriteWithoutBlocking(int fd);

 private:
  std::string name_;
  Domain domain_;
  Listener::Delegate* delegate_;
  bool started_;
  int fd_;
  int token_;
  Lock switchboard_lock_;
  static LeopardSwitchboardThread* g_switchboard_thread_;
  base::MessagePumpLibevent::FileDescriptorWatcher watcher_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  void StartLeopard();
  void StartSnowLeopard();
  DISALLOW_COPY_AND_ASSIGN(ListenerImpl);
};


LeopardSwitchboardThread::LeopardSwitchboardThread()
    : base::SimpleThread("LeopardSwitchboardThread"), internal_fd_(-1),
      external_fd_(-1), notify_fd_(-1), notify_fd_token_(-1), finished_(false) {
}

LeopardSwitchboardThread::~LeopardSwitchboardThread() {
  if (internal_fd_ != -1) {
    close(internal_fd_);
  }
  if (external_fd_ != -1) {
    close(external_fd_);
  }
  if (notify_fd_ != -1) {
    // Cancelling this notification takes care of closing notify_fd_.
    uint32_t status = notify_cancel(notify_fd_token_);
    DCHECK_EQ(status, static_cast<uint32_t>(NOTIFY_STATUS_OK));
  }
}

bool LeopardSwitchboardThread::Init() {
  // Create a pair of sockets for communicating with the thread
  // The file descriptors returned from socketpair can be kqueue'd on 10.5.
  int sockets[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
    PLOG(ERROR) << "socketpair";
    return false;
  }
  internal_fd_ = sockets[0];
  external_fd_ = sockets[1];

  // Register a bogus notification so that there is single notify_fd_ to
  // monitor. This runs a small risk of overflowing the notification buffer
  // if notifications are used heavily (see man 3 notify), however it greatly
  // simplifies the select loop code as there are only 2 file descriptors
  // that need to be monitored, and there is no need to add/remove file
  // descriptors from fd_set_ as listeners are added and removed.
  // This also keep the total fd usage on 10.5 to three for all
  // notifications. The 10.6 implementation will use one fd per notification,
  // but doesn't run the risk of notification buffer overflow. If fds ever
  // become tight, the 10.6 code could be changed to use only one fd for
  // all notifications.
  std::string notification = StringPrintf("LeopardSwitchboardThread.%d",
                                          getpid());
  notification = AddPrefixToNotification(notification, ProfileDomain);
  uint32_t status = notify_register_file_descriptor(
      notification.c_str(), &notify_fd_, 0, &notify_fd_token_);
  if (status != NOTIFY_STATUS_OK) {
    return false;
  }

  FD_ZERO(&fd_set_);
  FD_SET(internal_fd_, &fd_set_);
  FD_SET(notify_fd_, &fd_set_);

  MessageLoopForIO* io_loop = MessageLoopForIO::current();
  // Watch for destruction of the BrowserThread::IO message loop so that
  // the thread can be stopped cleanly.
  io_loop->AddDestructionObserver(this);
  return io_loop->WatchFileDescriptor(
      external_fd_, true, MessageLoopForIO::WATCH_READ, &watcher_, this);
}

void LeopardSwitchboardThread::WillDestroyCurrentMessageLoop() {
  DCHECK_EQ(MessageLoop::current(), MessageLoopForIO::current());
  watcher_.StopWatchingFileDescriptor();

  // Send the appropriate message to end our thread, and then wait for it
  // to finish before continuing.
  int message = kKillThreadMessage;
  write(external_fd_, &message, sizeof(message));
  Join();
}

void LeopardSwitchboardThread::Run() {
  DCHECK(!finished_);
  int nfds = std::max(internal_fd_, notify_fd_) + 1;
  while (1) {
    fd_set working_set;
    FD_COPY(&fd_set_, &working_set);
    int count = HANDLE_EINTR(select(nfds, &working_set, NULL, NULL, NULL));
    if (count < 0) {
      PLOG(ERROR) << "select";
      break;
    } else if (count == 0) {
      DLOG(INFO) << "select timed out";
      continue;
    }
    if (FD_ISSET(notify_fd_, &working_set)) {
      int token;
      int status = HANDLE_EINTR(read(notify_fd_, &token, sizeof(token)));
      if (status < 0) {
        PLOG(ERROR) << "read";
        break;
      } else if (status == 0) {
        LOG(ERROR) << "notify fd closed";
        break;
      } else if (status != sizeof(token)) {
        LOG(ERROR) << "read wrong size: " << status;
        break;
      } else if (token == notify_fd_token_) {
        LOG(ERROR) << "invalid token: " << token;
      }
      status = HANDLE_EINTR(write(internal_fd_, &token, sizeof(token)));
      if (status < 0) {
        PLOG(ERROR) << "write";
        break;
      } else if (status == 0) {
        LOG(ERROR) << "external_fd_ closed";
        break;
      } else if (status != sizeof(token)) {
        LOG(ERROR) << "write wrong size: " << status;
        break;
      }
    }
    if (FD_ISSET(internal_fd_, &working_set)) {
      int value;
      int status = HANDLE_EINTR(read(internal_fd_, &value, sizeof(value)));
      if (status < 0) {
        PLOG(ERROR) << "read";
      } else if (status == 0) {
        LOG(ERROR) << "internal_fd_ closed";
      } else if (value != kKillThreadMessage) {
        LOG(ERROR) << "unknown message sent: " << value;
      }
      break;
    }
  }
  finished_ = true;
}

bool LeopardSwitchboardThread::AddListener(ListenerImpl* listener,
                                           const std::string& notification) {
  DCHECK(!finished());
  base::AutoLock autolock(entries_lock_);
  for (std::vector<SwitchboardEntry>::iterator i = entries_.begin();
       i < entries_.end(); ++i) {
    if (i->listener_ == listener && i->notification_ == notification) {
      LOG(ERROR) << "listener " << listener
                 << " already registered for '" << notification << "'";
      return false;
    }
  }
  int token;
  uint32_t status = notify_register_file_descriptor(
     notification.c_str(), &notify_fd_, NOTIFY_REUSE, &token);
  if (status != NOTIFY_STATUS_OK) {
    LOG(ERROR) << "unable to notify_register_file_descriptor for '"
               << notification << "' status: " << status;
    return false;
  }
  SwitchboardEntry entry;
  entry.token_ = token;
  entry.notification_ = notification;
  entry.listener_ = listener;
  entries_.push_back(entry);
  return true;
}

bool LeopardSwitchboardThread::RemoveListener(ListenerImpl* listener,
                                              const std::string& notification) {
  DCHECK(!finished());
  base::AutoLock autolock(entries_lock_);
  for (std::vector<SwitchboardEntry>::iterator i = entries_.begin();
       i < entries_.end(); ++i) {
    if (i->listener_ == listener && i->notification_ == notification) {
      uint32_t status = notify_cancel(i->token_);
      DCHECK_EQ(status, static_cast<uint32_t>(NOTIFY_STATUS_OK));
      entries_.erase(i);
      return true;
    }
  }
  LOG(ERROR) << "unable to remove listener '" << listener
             << "' for '" << notification << "'.";
  return false;
}

void LeopardSwitchboardThread::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(MessageLoop::current(), MessageLoopForIO::current());
  DCHECK_EQ(fd, external_fd_);
  int token;
  int status = HANDLE_EINTR(read(fd, &token, sizeof(token)));
  if (status < 0) {
    PLOG(ERROR) << "read";
  } else if (status == 0) {
    LOG(ERROR) << "external_fd_ closed";
  } else if (status != sizeof(token)) {
    LOG(ERROR) << "unexpected read size " << status;
  } else {
    // Have to swap to native endianness <http://openradar.appspot.com/8821081>.
    token = static_cast<int>(ntohl(token));
    base::AutoLock autolock(entries_lock_);
    bool found_token = false;
    for (std::vector<SwitchboardEntry>::iterator i = entries_.begin();
         i < entries_.end(); ++i) {
      if (i->token_ == token) {
        found_token = true;
        i->listener_->OnListen();
      }
    }
    if (!found_token) {
      LOG(ERROR) << "read unknown token " << token;
    }
  }
}

void LeopardSwitchboardThread::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

LeopardSwitchboardThread* ListenerImpl::g_switchboard_thread_ = NULL;

ListenerImpl::ListenerImpl(
    const std::string& name, Domain domain, Listener::Delegate* delegate)
    : name_(name), domain_(domain), delegate_(delegate), started_(false),
      fd_(-1), token_(-1) {
}

ListenerImpl::~ListenerImpl() {
  if (started_) {
    if (!UseLeopardSwitchboardThread()) {
      if (fd_ != -1) {
        uint32_t status = notify_cancel(token_);
        DCHECK_EQ(status, static_cast<uint32_t>(NOTIFY_STATUS_OK));
      }
    } else {
      base::AutoLock autolock(switchboard_lock_);
      if (g_switchboard_thread_) {
        std::string notification = AddPrefixToNotification(name_, domain_);
        CHECK(g_switchboard_thread_->RemoveListener(this, notification));
      }
    }
  }
}

bool ListenerImpl::Start(MessageLoop* io_loop_to_listen_on) {
  DCHECK_EQ(fd_, -1);
  DCHECK_EQ(token_, -1);
  if (io_loop_to_listen_on->type() != MessageLoop::TYPE_IO) {
    DLOG(ERROR) << "io_loop_to_listen_on must be TYPE_IO";
    return false;
  }
  message_loop_proxy_ = base::MessageLoopProxy::CreateForCurrentThread();
  Task* task;
  if(UseLeopardSwitchboardThread()) {
    task = NewRunnableMethod(this, &ListenerImpl::StartLeopard);
  } else {
    task = NewRunnableMethod(this, &ListenerImpl::StartSnowLeopard);
  }
  io_loop_to_listen_on->PostTask(FROM_HERE, task);
  return true;
}

void ListenerImpl::StartLeopard() {
  DCHECK(UseLeopardSwitchboardThread());
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());
  bool success = true;
  {
    base::AutoLock autolock(switchboard_lock_);
    if (g_switchboard_thread_ && g_switchboard_thread_->HasBeenJoined()) {
      // The only time this should ever occur is in unit tests.
      delete g_switchboard_thread_;
      g_switchboard_thread_ = NULL;
    }
    DCHECK(!(g_switchboard_thread_ && g_switchboard_thread_->finished()));
    if (!g_switchboard_thread_) {
      g_switchboard_thread_ = new LeopardSwitchboardThread();
      success = g_switchboard_thread_->Init();
      if (success) {
        g_switchboard_thread_->Start();
      }
    }
    if (success) {
      std::string notification = AddPrefixToNotification(name_, domain_);
      success = g_switchboard_thread_->AddListener(this, notification);
    }
  }
  started_ = success;
  Task* task =
      new Listener::ListenerStartedTask(name_, domain_, delegate_, success);
  CHECK(message_loop_proxy_->PostTask(FROM_HERE, task));
}

void ListenerImpl::StartSnowLeopard() {
  DCHECK(!UseLeopardSwitchboardThread());
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());
  bool success = true;
  std::string notification = AddPrefixToNotification(name_, domain_);
  uint32_t status = notify_register_file_descriptor(
      notification.c_str(), &fd_, 0, &token_);
  if (status != NOTIFY_STATUS_OK) {
    LOG(ERROR) << "unable to notify_register_file_descriptor for '"
               << notification << "' Status: " << status;
    success = false;
  }
  if (success) {
    MessageLoopForIO* io_loop = MessageLoopForIO::current();
    success = io_loop->WatchFileDescriptor(
        fd_, true, MessageLoopForIO::WATCH_READ, &watcher_, this);
    if (!success) {
      uint32_t status = notify_cancel(token_);
      DCHECK_EQ(status, static_cast<uint32_t>(NOTIFY_STATUS_OK));
      fd_ = -1;
    }
  }
  started_ = success;
  Task* task =
      new Listener::ListenerStartedTask(name_, domain_, delegate_, success);
  CHECK(message_loop_proxy_->PostTask(FROM_HERE, task));
}

void ListenerImpl::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(!UseLeopardSwitchboardThread());
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());
  DCHECK_EQ(fd, fd_);
  int token;
  int status = HANDLE_EINTR(read(fd, &token, sizeof(token)));
  if (status < 0) {
    PLOG(ERROR) << "read";
  } else if (status == 0) {
    LOG(ERROR) << "external_fd_ closed";
  } else if (status != sizeof(token)) {
    LOG(ERROR) << "unexpected read size " << status;
  } else {
    // Have to swap to native endianness <http://openradar.appspot.com/8821081>.
    token = static_cast<int>(ntohl(token));
    if (token == token_) {
      OnListen();
    } else {
      LOG(ERROR) << "unexpected value " << token;
    }
  }
}

void ListenerImpl::OnListen() {
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());
  Task* task =
      new Listener::NotificationReceivedTask(name_, domain_, delegate_);
  CHECK(message_loop_proxy_->PostTask(FROM_HERE, task));
}

void ListenerImpl::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

Listener::Listener(
    const std::string& name, Domain domain, Listener::Delegate* delegate)
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

DISABLE_RUNNABLE_METHOD_REFCOUNT(multi_process_notification::ListenerImpl);
