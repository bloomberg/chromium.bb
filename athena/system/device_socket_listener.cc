// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/system/device_socket_listener.h"

#include <errno.h>
#include <map>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "ipc/unix_domain_socket_util.h"

namespace athena {

namespace {

typedef ObserverList<DeviceSocketListener> DeviceSocketListeners;

// Reads from a device socket blocks of a particular size. When that amount of
// data is read DeviceSocketManager::OnDataAvailable is called on the singleton
// instance which then informs all of the listeners on that socket.
class DeviceSocketReader : public base::MessagePumpLibevent::Watcher {
 public:
  DeviceSocketReader(const std::string& socket_path,
                      size_t data_size)
      : socket_path_(socket_path),
        data_size_(data_size),
        data_(new char[data_size]) {
  }
  virtual ~DeviceSocketReader() {}

 private:
  // Overidden from base::MessagePumpLibevent::Watcher.
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

  std::string socket_path_;
  size_t data_size_;
  scoped_ptr<char[]> data_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSocketReader);
};

class DeviceSocketManager;
DeviceSocketManager* device_socket_manager_instance_ = NULL;

// A singleton instance for managing all connections to sockets.
class DeviceSocketManager {
 public:
  static void Create(scoped_refptr<base::TaskRunner> file_task_runner) {
    device_socket_manager_instance_ = new DeviceSocketManager(file_task_runner);
  }

  static void Shutdown() {
    CHECK(device_socket_manager_instance_);
    device_socket_manager_instance_->ScheduleDelete();
    // Once scheduled to be deleted, no-one should be
    // able to access it.
    device_socket_manager_instance_ = NULL;
  }

  static DeviceSocketManager* GetInstanceUnsafe() {
    return device_socket_manager_instance_;
  }

  static DeviceSocketManager* GetInstance() {
    CHECK(device_socket_manager_instance_);
    return device_socket_manager_instance_;
  }

  // If there isn't an existing connection to |socket_path|, then opens a
  // connection to |socket_path| and starts listening for data. All listeners
  // for |socket_path| receives data when data is available on the socket.
  void StartListening(const std::string& socket_path,
                      size_t data_size,
                      DeviceSocketListener* listener);

  // Removes |listener| from the list of listeners that receive data from
  // |socket_path|. If this is the last listener, then this closes the
  // connection to the socket.
  void StopListening(const std::string& socket_path,
                     DeviceSocketListener* listener);

  // Sends data to all the listeners registered to receive data from
  // |socket_path|.
  void OnDataAvailable(const std::string& socket_path,
                       const void* data);

  // Notifies listeners of errors reading from the socket and closes it.
  void OnError(const std::string& socket_path, int err);
  void OnEOF(const std::string& socket_path);

 private:
  struct SocketData {
    SocketData()
        : fd(-1) {
    }

    int fd;
    DeviceSocketListeners observers;
    scoped_ptr<base::MessagePumpLibevent::FileDescriptorWatcher> controller;
    scoped_ptr<DeviceSocketReader> watcher;
  };

  static void DeleteOnFILE(DeviceSocketManager* manager) { delete manager; }

  DeviceSocketManager(scoped_refptr<base::TaskRunner> file_task_runner)
      : file_task_runner_(file_task_runner) {}

  ~DeviceSocketManager() {
    STLDeleteContainerPairSecondPointers(socket_data_.begin(),
                                         socket_data_.end());
  }

  void ScheduleDelete();

  void StartListeningOnFILE(const std::string& socket_path,
                            size_t data_size,
                            DeviceSocketListener* listener);

  void StopListeningOnFILE(const std::string& socket_path,
                           DeviceSocketListener* listener);

  void CloseSocket(const std::string& socket_path);

  std::map<std::string, SocketData*> socket_data_;
  scoped_refptr<base::TaskRunner> file_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSocketManager);
};

////////////////////////////////////////////////////////////////////////////////
// DeviceSocketReader

void DeviceSocketReader::OnFileCanReadWithoutBlocking(int fd) {
  ssize_t read_size = recv(fd, data_.get(), data_size_, 0);
  if (read_size < 0) {
    if (errno == EINTR)
      return;
    DeviceSocketManager::GetInstance()->OnError(socket_path_, errno);
    return;
  }
  if (read_size == 0) {
    DeviceSocketManager::GetInstance()->OnEOF(socket_path_);
    return;
  }
  if (read_size != static_cast<ssize_t>(data_size_))
    return;
  DeviceSocketManager::GetInstance()->OnDataAvailable(socket_path_,
                                                      data_.get());
}

void DeviceSocketReader::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// DeviceSocketManager

void DeviceSocketManager::StartListening(const std::string& socket_path,
                                         size_t data_size,
                                         DeviceSocketListener* listener) {
  file_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DeviceSocketManager::StartListeningOnFILE,
                 base::Unretained(this),
                 socket_path,
                 data_size,
                 listener));
}

void DeviceSocketManager::StopListening(const std::string& socket_path,
                                        DeviceSocketListener* listener) {
  file_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DeviceSocketManager::StopListeningOnFILE,
                 base::Unretained(this),
                 socket_path,
                 listener));
}

void DeviceSocketManager::OnDataAvailable(const std::string& socket_path,
                                          const void* data) {
  CHECK_GT(socket_data_.count(socket_path), 0UL);
  DeviceSocketListeners& listeners = socket_data_[socket_path]->observers;
  FOR_EACH_OBSERVER(
      DeviceSocketListener, listeners, OnDataAvailableOnFILE(data));
}

void DeviceSocketManager::CloseSocket(const std::string& socket_path) {
  if (!socket_data_.count(socket_path))
    return;
  SocketData* socket_data = socket_data_[socket_path];
  close(socket_data->fd);
  delete socket_data;
  socket_data_.erase(socket_path);
}

void DeviceSocketManager::OnError(const std::string& socket_path, int err) {
  LOG(ERROR) << "Error reading from socket: " << socket_path << ": "
             << strerror(err);
  CloseSocket(socket_path);
  // TODO(flackr): Notify listeners that the socket was closed unexpectedly.
}

void DeviceSocketManager::OnEOF(const std::string& socket_path) {
  LOG(ERROR) << "EOF reading from socket: " << socket_path;
  CloseSocket(socket_path);
}

void DeviceSocketManager::StartListeningOnFILE(const std::string& socket_path,
                                               size_t data_size,
                                               DeviceSocketListener* listener) {
  CHECK(file_task_runner_->RunsTasksOnCurrentThread());
  SocketData* socket_data = NULL;
  if (!socket_data_.count(socket_path)) {
    int socket_fd = -1;
    if (!IPC::CreateClientUnixDomainSocket(base::FilePath(socket_path),
                                           &socket_fd)) {
      LOG(ERROR) << "Error connecting to socket: " << socket_path;
      return;
    }

    socket_data = new SocketData;
    socket_data_[socket_path] = socket_data;

    socket_data->fd = socket_fd;

    socket_data->controller.reset(
        new base::MessagePumpLibevent::FileDescriptorWatcher());
    socket_data->watcher.reset(
        new DeviceSocketReader(socket_path, data_size));

    base::MessageLoopForIO::current()->WatchFileDescriptor(
        socket_fd,
        true,
        base::MessageLoopForIO::WATCH_READ,
        socket_data->controller.get(),
        socket_data->watcher.get());
  } else {
    socket_data = socket_data_[socket_path];
  }
  socket_data->observers.AddObserver(listener);
}

void DeviceSocketManager::StopListeningOnFILE(const std::string& socket_path,
                                              DeviceSocketListener* listener) {
  if (!socket_data_.count(socket_path))
    return;  // Happens if unable to create a socket.

  CHECK(file_task_runner_->RunsTasksOnCurrentThread());
  DeviceSocketListeners& listeners = socket_data_[socket_path]->observers;
  listeners.RemoveObserver(listener);
  if (!listeners.might_have_observers()) {
    // All listeners for this socket has been removed. Close the socket.
    CloseSocket(socket_path);
  }
}

void DeviceSocketManager::ScheduleDelete() {
  // Schedule a task to delete on FILE thread because
  // there may be a task scheduled on |file_task_runner_|.
  file_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DeleteOnFILE, base::Unretained(this)));
}

}  // namespace

DeviceSocketListener::DeviceSocketListener(const std::string& socket_path,
                                           size_t data_size)
  : socket_path_(socket_path),
    data_size_(data_size) {
}

DeviceSocketListener::~DeviceSocketListener() {
  StopListening();
}

// static
void DeviceSocketListener::CreateSocketManager(
    scoped_refptr<base::TaskRunner> file_task_runner) {
  DeviceSocketManager::Create(file_task_runner);
}

// static
void DeviceSocketListener::ShutdownSocketManager() {
  DeviceSocketManager::Shutdown();
}

void DeviceSocketListener::StartListening() {
  DeviceSocketManager::GetInstance()->StartListening(socket_path_,
                                                     data_size_,
                                                     this);
}

void DeviceSocketListener::StopListening() {
  DeviceSocketManager* instance = DeviceSocketManager::GetInstanceUnsafe();
  if (instance)
    instance->StopListening(socket_path_, this);
}

}  // namespace athena
