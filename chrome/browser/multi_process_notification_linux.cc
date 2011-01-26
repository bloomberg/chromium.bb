// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multi_process_notification.h"

#include <dbus/dbus.h>
#include <set>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/stringprintf.h"
#include "base/threading/simple_thread.h"
#include "chrome/common/chrome_paths.h"

namespace multi_process_notification {

const char* kNotifyPath = "/modules/notify";
const char* kNotifyInterface = "org.chromium.Notify";
const char* kNotifySignalName = "Notify";

// A simple thread to run the DBus main loop.
class ListenerThread : public base::SimpleThread {
 public:
  ListenerThread();

  bool Init();

  bool AddListener(ListenerImpl* listner);
  bool RemoveListener(ListenerImpl* listner);

  // SimpleThread overrides
  virtual void Run();

 private:
  friend struct DefaultSingletonTraits<ListenerThread>;

  DBusConnection* connection_;

  // Access to this needs to be protected by ListenerThreadLock::GetLock().
  std::set<ListenerImpl*> listeners_;

  DISALLOW_COPY_AND_ASSIGN(ListenerThread);
};

// This is a singleton class that owns the lock for the ListenerThread and
// the thread.
class ListenerThreadLock {
 public:
  static ListenerThreadLock* GetInstance();

  // Return a lock that can be used to protect access to the ListenerThread's
  // member variables.
  base::Lock& GetLock();

  // Return the global ListenerThread, creating/initializing it as needed.
  // Note that this should not be called if you've taken the lock (using
  // GetLock()) since this call needs to make use of that lock.
  ListenerThread* GetThread();

 private:
  ListenerThreadLock();
  friend struct DefaultSingletonTraits<ListenerThreadLock>;

  // This lock is used to protect the thread creation and the thread members.
  base::Lock thread_lock_;

  ListenerThread* thread_;

  DISALLOW_COPY_AND_ASSIGN(ListenerThreadLock);
};

// This does all the heavy lifting for Listener class.
class ListenerImpl {
 public:
  ListenerImpl(const std::string& name,
               Domain domain,
               Listener::Delegate* delegate);
  ~ListenerImpl();

  bool Start(MessageLoop* io_loop_to_listen_on);
  void OnListen(const std::string& name);

  std::string name() const { return name_; }
  Domain domain() const { return domain_; }

 private:
  void StartListener();

  std::string name_;
  Domain domain_;
  Listener::Delegate* delegate_;

  DBusError error_;
  DBusConnection* connection_;

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ListenerImpl);
};

// Return the fullname for this signal by taking the user-specified name and
// adding a prefix based on the domain.
std::string AddDomainPrefixToNotification(const std::string& name,
                                          Domain domain) {
  std::string prefix;
  switch (domain) {
    case multi_process_notification::ProfileDomain: {
      FilePath user_data_dir;
      if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
        NOTREACHED();
      }
      prefix = StringPrintf("user.%u.%s.", getuid(),
                            user_data_dir.value().c_str());
      break;
    }
    case multi_process_notification::UserDomain:
      prefix = StringPrintf("user.%u.", getuid());
      break;
    case multi_process_notification::SystemDomain:
      prefix = "";
      break;
  }
  return prefix + name;
}

bool Post(const std::string& name, Domain domain) {
  DBusError error;
  dbus_error_init(&error);

  // Get a connection to the DBus.
  DBusConnection* connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
  if (dbus_error_is_set(&error)) {
    LOG(ERROR) << "Failed to create initial dbus connection: " << error.message;
    dbus_error_free(&error);
    return false;
  }
  if (!connection) {
    LOG(ERROR) << "Failed to create initial dbus connection";
    return false;
  }

  std::string fullname = AddDomainPrefixToNotification(name, domain);

  // Create the Notify signal.
  bool success = true;
  DBusMessage* message = dbus_message_new_signal(kNotifyPath, kNotifyInterface,
                                                 kNotifySignalName);
  if (!message) {
    LOG(ERROR) << "Failed to create dbus message for signal: "
               << kNotifySignalName << " (" << kNotifyInterface << ")";
    success = false;
  }

  // Add the full signal name as an argument to the Notify signal.
  if (success) {
    DBusMessageIter args;
    const char* cname = fullname.c_str();
    dbus_message_iter_init_append(message, &args);
    if (!dbus_message_iter_append_basic(&args,
                                        DBUS_TYPE_STRING, &cname)) {
      LOG(ERROR) << "Failed to set signal name: " << fullname;
      success = false;
    }
  }

  // Actually send the signal.
  if (success) {
    dbus_uint32_t serial = 0;
    if (!dbus_connection_send(connection, message, &serial)) {
      LOG(ERROR) << "Unable to send dbus message for " << fullname;
      success = false;
    }
  }

  if (success) {
    dbus_connection_flush(connection);
  }
  dbus_message_unref(message);

  return success;
}

ListenerThread::ListenerThread()
    : base::SimpleThread("ListenerThread"),
      connection_(NULL) {
}

bool ListenerThread::Init() {
  DBusError error;
  dbus_error_init(&error);

  // Get a connection to the DBus.
  connection_ = dbus_bus_get(DBUS_BUS_SESSION, &error);
  if (dbus_error_is_set(&error)) {
    LOG(ERROR) << "Failed to create initial dbus connection: " << error.message;
    dbus_error_free(&error);
    return false;
  }
  if (!connection_) {
    LOG(ERROR) << "Failed to create initial dbus connection";
    return false;
  }

  // Create matching rule for our signal type.
  std::string match_rule = StringPrintf("type='signal',interface='%s'",
                                        kNotifyInterface);
  dbus_bus_add_match(connection_, match_rule.c_str(), &error);
  dbus_connection_flush(connection_);
  if (dbus_error_is_set(&error)) {
    LOG(ERROR) << "Failed to add dbus match rule for "
               << kNotifyInterface << ": "
               << error.message;
    dbus_error_free(&error);
    return false;
  }

  return true;
}

bool ListenerThread::AddListener(ListenerImpl* listener) {
  base::AutoLock autolock(ListenerThreadLock::GetInstance()->GetLock());
  listeners_.insert(listener);
  return true;
}

bool ListenerThread::RemoveListener(ListenerImpl* listener) {
  base::AutoLock autolock(ListenerThreadLock::GetInstance()->GetLock());
  listeners_.erase(listener);
  return true;
}

void ListenerThread::Run() {
  while (true) {
    // Get next available message.
    // Using -1 for the timeout (instead of 0) would make this a blocking call.
    // But it would unfortunately block *all* dbus calls on this connection.
    // Thus, we use a timeout of 0 and a (nano)sleep.
    // TODO(garykac): Ugh. Fix this to avoid the call to sleep if possible.
    dbus_connection_read_write(connection_, 0);

    DBusMessage* message = dbus_connection_pop_message(connection_);
    if (!message) {
      struct timespec delay = {0};
      delay.tv_sec = 0;
      delay.tv_nsec = 500 * 1000 * 1000;
      nanosleep(&delay, NULL);  // nanoYuck!
      continue;
    }

    // Process all queued up messages.
    while (message) {
      if (dbus_message_is_signal(message,
                                 kNotifyInterface, kNotifySignalName)) {
        // Get user-defined name from the signal arguments.
        DBusMessageIter args;
        if (!dbus_message_iter_init(message, &args)) {
          LOG(ERROR) << "Params missing from dbus signal";
        } else if (dbus_message_iter_get_arg_type(&args) != DBUS_TYPE_STRING) {
          LOG(ERROR) << "Dbus signal param is not string type";
        } else {
          const char* name;
          dbus_message_iter_get_basic(&args, &name);
          {  // Scope for lock
            base::AutoLock autolock(ListenerThreadLock::GetInstance()
                                    ->GetLock());
            std::set<ListenerImpl*>::iterator it;
            // Note that this doesn't scale well to a large number of listeners.
            // But we should only have a couple active at a time.
            for (it=listeners_.begin(); it!=listeners_.end(); it++) {
              (*it)->OnListen(name);
            }
          }
        }
      }

      dbus_message_unref(message);
      message = dbus_connection_pop_message(connection_);
    }
  }
}

ListenerImpl::ListenerImpl(const std::string& name,
                           Domain domain,
                           Listener::Delegate* delegate)
    : name_(name),
      domain_(domain),
      delegate_(delegate),
      connection_(NULL) {
}

ListenerImpl::~ListenerImpl() {
  ListenerThreadLock* threadlock = ListenerThreadLock::GetInstance();
  ListenerThread* thread = threadlock->GetThread();
  if (thread) {
    thread->RemoveListener(this);
  }
}

bool ListenerImpl::Start(MessageLoop* io_loop_to_listen_on) {
  if (io_loop_to_listen_on->type() != MessageLoop::TYPE_IO) {
    DLOG(ERROR) << "io_loop_to_listen_on must be TYPE_IO";
    return false;
  }

  // Start the listener on the IO thread.
  message_loop_proxy_ = base::MessageLoopProxy::CreateForCurrentThread();
  Task* task = NewRunnableMethod(this, &ListenerImpl::StartListener);
  io_loop_to_listen_on->PostTask(FROM_HERE, task);
  return true;
}

void ListenerImpl::StartListener() {
  DCHECK_EQ(MessageLoop::TYPE_IO, MessageLoop::current()->type());

  ListenerThreadLock* threadlock = ListenerThreadLock::GetInstance();
  ListenerThread* thread = threadlock->GetThread();

  // Register ourselves as a listener of signals.
  bool success = false;
  if (thread) {
    success = thread->AddListener(this);
  }

  // Send initialization success/fail status to delegate.
  Task* task = new Listener::ListenerStartedTask(name_, domain_, delegate_,
                                                 success);
  CHECK(message_loop_proxy_->PostTask(FROM_HERE, task));
}

void ListenerImpl::OnListen(const std::string& name) {
  // Ignore the signal unless it matches our name.
  std::string fullname = AddDomainPrefixToNotification(name_, domain_);
  if (name == fullname) {
    Task* task =
        new Listener::NotificationReceivedTask(name_, domain_, delegate_);
    CHECK(message_loop_proxy_->PostTask(FROM_HERE, task));
  }
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

ListenerThreadLock* ListenerThreadLock::GetInstance() {
  return Singleton<ListenerThreadLock>::get();
}

ListenerThreadLock::ListenerThreadLock()
    : thread_(NULL) {
}

base::Lock& ListenerThreadLock::GetLock() {
  return thread_lock_;
}

ListenerThread* ListenerThreadLock::GetThread() {
  base::AutoLock autolock(thread_lock_);
  if (!thread_) {
    thread_ = new ListenerThread();
    if (!thread_) {
      LOG(ERROR) << "Unable to create ListenerThread";
      return NULL;
    }
    if (thread_->Init()) {
      thread_->Start();
    } else {
      LOG(ERROR) << "Unable to start dbus ListenerThread";
      delete thread_;
      thread_ = NULL;
    }
  }
  return thread_;
}

}  // namespace multi_process_notification

DISABLE_RUNNABLE_METHOD_REFCOUNT(multi_process_notification::ListenerImpl);
