// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/braille_display_private/brlapi_connection.h"

#include "base/chromeos/chromeos_version.h"
#include "base/message_loop/message_loop.h"

namespace extensions {
using base::MessageLoopForIO;
namespace api {
namespace braille_display_private {

namespace {
// Default virtual terminal.  This can be overriden by setting the
// WINDOWPATH environment variable.  This is only used when not running
// under Crhome OS (that is in aura for a Linux desktop).
// TODO(plundblad): Find a way to detect the controlling terminal of the
// X server.
static const int kDefaultTtyLinux = 7;
#if defined(OS_CHROMEOS)
// The GUI is always running on vt1 in Chrome OS.
static const int kDefaultTtyChromeOS = 1;
#endif
}  // namespace

class BrlapiConnectionImpl : public BrlapiConnection,
                             MessageLoopForIO::Watcher {
 public:
  explicit BrlapiConnectionImpl(LibBrlapiLoader* loader) :
      libbrlapi_loader_(loader) {}

  virtual ~BrlapiConnectionImpl() {
    Disconnect();
  }

  virtual bool Connect(const OnDataReadyCallback& on_data_ready) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool Connected() OVERRIDE { return handle_; }
  virtual brlapi_error_t* BrlapiError() OVERRIDE;
  virtual std::string BrlapiStrError() OVERRIDE;
  virtual bool GetDisplaySize(size_t* size) OVERRIDE;
  virtual bool WriteDots(const unsigned char* cells) OVERRIDE;
  virtual int ReadKey(brlapi_keyCode_t* keyCode) OVERRIDE;

  // MessageLoopForIO::Watcher
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE {
    on_data_ready_.Run();
  }

  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE {}

 private:
  bool CheckConnected();

  LibBrlapiLoader* libbrlapi_loader_;
  scoped_ptr<brlapi_handle_t, base::FreeDeleter> handle_;
  MessageLoopForIO::FileDescriptorWatcher fd_controller_;
  OnDataReadyCallback on_data_ready_;

  DISALLOW_COPY_AND_ASSIGN(BrlapiConnectionImpl);
};

BrlapiConnection::BrlapiConnection() {
}

BrlapiConnection::~BrlapiConnection() {
}

scoped_ptr<BrlapiConnection> BrlapiConnection::Create(
    LibBrlapiLoader* loader) {
  DCHECK(loader->loaded());
  return scoped_ptr<BrlapiConnection>(new BrlapiConnectionImpl(loader));
}

bool BrlapiConnectionImpl::Connect(const OnDataReadyCallback& on_data_ready) {
  DCHECK(!handle_);
  handle_.reset((brlapi_handle_t*) malloc(
      libbrlapi_loader_->brlapi_getHandleSize()));
  int fd = libbrlapi_loader_->brlapi__openConnection(handle_.get(), NULL, NULL);
  if (fd < 0) {
    handle_.reset();
    LOG(ERROR) << "Error connecting to brlapi: " << BrlapiStrError();
    return false;
  }
  int path[2] = {0, 0};
  int pathElements = 0;
#if defined(OS_CHROMEOS)
  if (base::chromeos::IsRunningOnChromeOS())
    path[pathElements++] = kDefaultTtyChromeOS;
#endif
  if (pathElements == 0 && getenv("WINDOWPATH") == NULL)
    path[pathElements++] = kDefaultTtyLinux;
  if (libbrlapi_loader_->brlapi__enterTtyModeWithPath(
          handle_.get(), path, pathElements, NULL) < 0) {
    LOG(ERROR) << "brlapi: couldn't enter tty mode: " << BrlapiStrError();
    Disconnect();
    return false;
  }

  size_t size;
  if (!GetDisplaySize(&size)) {
    // Error already logged.
    Disconnect();
    return false;
  }

  // A display size of 0 means no display connected.  We can't reliably

  // detect when a display gets connected, so fail and let the caller
  // retry connecting.
  if (size == 0) {
    LOG(ERROR) << "No braille display connected";
    Disconnect();
    return false;
  }

  const brlapi_keyCode_t extraKeys[] = {
    BRLAPI_KEY_TYPE_CMD | BRLAPI_KEY_CMD_OFFLINE,
  };
  if (libbrlapi_loader_->brlapi__acceptKeys(
          handle_.get(), brlapi_rangeType_command, extraKeys,
          arraysize(extraKeys)) < 0) {
    LOG(ERROR) << "Couldn't acceptKeys: " << BrlapiStrError();
    Disconnect();
    return false;
  }

  if (!MessageLoopForIO::current()->WatchFileDescriptor(
          fd, true, MessageLoopForIO::WATCH_READ, &fd_controller_, this)) {
    LOG(ERROR) << "Couldn't watch file descriptor " << fd;
    Disconnect();
    return false;
  }

  on_data_ready_ = on_data_ready;

  return true;
}

void BrlapiConnectionImpl::Disconnect() {
  if (!handle_) {
    return;
  }
  fd_controller_.StopWatchingFileDescriptor();
  libbrlapi_loader_->brlapi__closeConnection(
      handle_.get());
  handle_.reset();
}

brlapi_error_t* BrlapiConnectionImpl::BrlapiError() {
  return libbrlapi_loader_->brlapi_error_location();
}

std::string BrlapiConnectionImpl::BrlapiStrError() {
  return libbrlapi_loader_->brlapi_strerror(BrlapiError());
}

bool BrlapiConnectionImpl::GetDisplaySize(size_t* size) {
  if (!CheckConnected()) {
    return false;
  }
  unsigned int columns, rows;
  if (libbrlapi_loader_->brlapi__getDisplaySize(
          handle_.get(), &columns, &rows) < 0) {
    LOG(ERROR) << "Couldn't get braille display size " << BrlapiStrError();
    return false;
  }
  *size = columns * rows;
  return true;
}

bool BrlapiConnectionImpl::WriteDots(const unsigned char* cells) {
  if (!CheckConnected())
    return false;
  if (libbrlapi_loader_->brlapi__writeDots(handle_.get(), cells) < 0) {
    LOG(ERROR) << "Couldn't write to brlapi: " << BrlapiStrError();
    return false;
  }
  return true;
}

int BrlapiConnectionImpl::ReadKey(brlapi_keyCode_t* key_code) {
  if (!CheckConnected())
    return -1;
  return libbrlapi_loader_->brlapi__readKey(
      handle_.get(), 0 /*wait*/, key_code);
}

bool BrlapiConnectionImpl::CheckConnected() {
  if (!handle_) {
    BrlapiError()->brlerrno = BRLAPI_ERROR_ILLEGAL_INSTRUCTION;
    return false;
  }
  return true;
}

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions
