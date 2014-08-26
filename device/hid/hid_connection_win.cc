// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection_win.h"

#include <cstring>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/message_loop/message_loop.h"
#include "base/win/object_watcher.h"

#define INITGUID

#include <windows.h>
#include <hidclass.h>

extern "C" {
#include <hidsdi.h>
}

#include <setupapi.h>
#include <winioctl.h>

namespace device {

struct PendingHidTransfer : public base::RefCounted<PendingHidTransfer>,
                            public base::win::ObjectWatcher::Delegate,
                            public base::MessageLoop::DestructionObserver {
  typedef base::Callback<void(PendingHidTransfer*, bool)> Callback;

  PendingHidTransfer(scoped_refptr<net::IOBuffer> buffer,
                     const Callback& callback);

  void TakeResultFromWindowsAPI(BOOL result);

  OVERLAPPED* GetOverlapped() { return &overlapped_; }

  // Implements base::win::ObjectWatcher::Delegate.
  virtual void OnObjectSignaled(HANDLE object) OVERRIDE;

  // Implements base::MessageLoop::DestructionObserver
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  // The buffer isn't used by this object but it's important that a reference
  // to it is held until the transfer completes.
  scoped_refptr<net::IOBuffer> buffer_;
  Callback callback_;
  OVERLAPPED overlapped_;
  base::win::ScopedHandle event_;
  base::win::ObjectWatcher watcher_;

 private:
  friend class base::RefCounted<PendingHidTransfer>;

  virtual ~PendingHidTransfer();

  DISALLOW_COPY_AND_ASSIGN(PendingHidTransfer);
};

PendingHidTransfer::PendingHidTransfer(
    scoped_refptr<net::IOBuffer> buffer,
    const PendingHidTransfer::Callback& callback)
    : buffer_(buffer),
      callback_(callback),
      event_(CreateEvent(NULL, FALSE, FALSE, NULL)) {
  memset(&overlapped_, 0, sizeof(OVERLAPPED));
  overlapped_.hEvent = event_.Get();
}

PendingHidTransfer::~PendingHidTransfer() {
  base::MessageLoop::current()->RemoveDestructionObserver(this);
}

void PendingHidTransfer::TakeResultFromWindowsAPI(BOOL result) {
  if (result) {
    callback_.Run(this, true);
  } else if (GetLastError() == ERROR_IO_PENDING) {
    base::MessageLoop::current()->AddDestructionObserver(this);
    AddRef();
    watcher_.StartWatching(event_.Get(), this);
  } else {
    VPLOG(1) << "HID transfer failed";
    callback_.Run(this, false);
  }
}

void PendingHidTransfer::OnObjectSignaled(HANDLE event_handle) {
  callback_.Run(this, true);
  Release();
}

void PendingHidTransfer::WillDestroyCurrentMessageLoop() {
  watcher_.StopWatching();
  callback_.Run(this, false);
}

HidConnectionWin::HidConnectionWin(const HidDeviceInfo& device_info)
    : HidConnection(device_info) {
  file_.Set(CreateFileA(device_info.device_id.c_str(),
                        GENERIC_WRITE | GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED,
                        NULL));

  if (!file_.IsValid() &&
      GetLastError() == base::File::FILE_ERROR_ACCESS_DENIED) {
    file_.Set(CreateFileA(device_info.device_id.c_str(),
                          GENERIC_READ,
                          FILE_SHARE_READ,
                          NULL,
                          OPEN_EXISTING,
                          FILE_FLAG_OVERLAPPED,
                          NULL));
  }
}

HidConnectionWin::~HidConnectionWin() {
  CancelIo(file_.Get());
}

void HidConnectionWin::PlatformRead(
    const HidConnection::ReadCallback& callback) {
  // Windows will always include the report ID (including zero if report IDs
  // are not in use) in the buffer.
  scoped_refptr<net::IOBufferWithSize> buffer =
      new net::IOBufferWithSize(device_info().max_input_report_size + 1);
  scoped_refptr<PendingHidTransfer> transfer(new PendingHidTransfer(
      buffer,
      base::Bind(&HidConnectionWin::OnReadComplete, this, buffer, callback)));
  transfers_.insert(transfer);
  transfer->TakeResultFromWindowsAPI(
      ReadFile(file_.Get(),
               buffer->data(),
               static_cast<DWORD>(buffer->size()),
               NULL,
               transfer->GetOverlapped()));
}

void HidConnectionWin::PlatformWrite(scoped_refptr<net::IOBuffer> buffer,
                                     size_t size,
                                     const WriteCallback& callback) {
  // The Windows API always wants either a report ID (if supported) or
  // zero at the front of every output report.
  scoped_refptr<PendingHidTransfer> transfer(new PendingHidTransfer(
      buffer, base::Bind(&HidConnectionWin::OnWriteComplete, this, callback)));
  transfers_.insert(transfer);
  transfer->TakeResultFromWindowsAPI(WriteFile(file_.Get(),
                                               buffer->data(),
                                               static_cast<DWORD>(size),
                                               NULL,
                                               transfer->GetOverlapped()));
}

void HidConnectionWin::PlatformGetFeatureReport(uint8_t report_id,
                                                const ReadCallback& callback) {
  // The first byte of the destination buffer is the report ID being requested.
  scoped_refptr<net::IOBufferWithSize> buffer =
      new net::IOBufferWithSize(device_info().max_feature_report_size + 1);
  buffer->data()[0] = report_id;

  scoped_refptr<PendingHidTransfer> transfer(new PendingHidTransfer(
      buffer,
      base::Bind(
          &HidConnectionWin::OnReadFeatureComplete, this, buffer, callback)));
  transfers_.insert(transfer);
  transfer->TakeResultFromWindowsAPI(
      DeviceIoControl(file_.Get(),
                      IOCTL_HID_GET_FEATURE,
                      NULL,
                      0,
                      buffer->data(),
                      static_cast<DWORD>(buffer->size()),
                      NULL,
                      transfer->GetOverlapped()));
}

void HidConnectionWin::PlatformSendFeatureReport(
    scoped_refptr<net::IOBuffer> buffer,
    size_t size,
    const WriteCallback& callback) {
  // The Windows API always wants either a report ID (if supported) or
  // zero at the front of every output report.
  scoped_refptr<PendingHidTransfer> transfer(new PendingHidTransfer(
      buffer, base::Bind(&HidConnectionWin::OnWriteComplete, this, callback)));
  transfer->TakeResultFromWindowsAPI(
      DeviceIoControl(file_.Get(),
                      IOCTL_HID_SET_FEATURE,
                      buffer->data(),
                      static_cast<DWORD>(size),
                      NULL,
                      0,
                      NULL,
                      transfer->GetOverlapped()));
}

void HidConnectionWin::OnReadComplete(scoped_refptr<net::IOBuffer> buffer,
                                      const ReadCallback& callback,
                                      PendingHidTransfer* transfer,
                                      bool signaled) {
  if (!signaled) {
    callback.Run(false, NULL, 0);
    return;
  }

  DWORD bytes_transferred;
  if (GetOverlappedResult(
          file_, transfer->GetOverlapped(), &bytes_transferred, FALSE)) {
    CompleteRead(buffer, bytes_transferred, callback);
  } else {
    VPLOG(1) << "HID read failed";
    callback.Run(false, NULL, 0);
  }
}

void HidConnectionWin::OnReadFeatureComplete(
    scoped_refptr<net::IOBuffer> buffer,
    const ReadCallback& callback,
    PendingHidTransfer* transfer,
    bool signaled) {
  if (!signaled) {
    callback.Run(false, NULL, 0);
    return;
  }

  DWORD bytes_transferred;
  if (GetOverlappedResult(
          file_, transfer->GetOverlapped(), &bytes_transferred, FALSE)) {
    scoped_refptr<net::IOBuffer> new_buffer(
        new net::IOBuffer(bytes_transferred - 1));
    memcpy(new_buffer->data(), buffer->data() + 1, bytes_transferred - 1);
    CompleteRead(new_buffer, bytes_transferred, callback);
  } else {
    VPLOG(1) << "HID read failed";
    callback.Run(false, NULL, 0);
  }
}

void HidConnectionWin::OnWriteComplete(const WriteCallback& callback,
                                       PendingHidTransfer* transfer,
                                       bool signaled) {
  if (!signaled) {
    callback.Run(false);
    return;
  }

  DWORD bytes_transferred;
  if (GetOverlappedResult(
          file_, transfer->GetOverlapped(), &bytes_transferred, FALSE)) {
    callback.Run(true);
  } else {
    VPLOG(1) << "HID write failed";
    callback.Run(false);
  }
}

}  // namespace device
