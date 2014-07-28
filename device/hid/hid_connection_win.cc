// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection_win.h"

#include <cstring>

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
  PendingHidTransfer(scoped_refptr<HidConnectionWin> connection,
                     scoped_refptr<net::IOBufferWithSize> target_buffer,
                     scoped_refptr<net::IOBufferWithSize> receive_buffer,
                     HidConnection::IOCallback callback);

  void TakeResultFromWindowsAPI(BOOL result);

  OVERLAPPED* GetOverlapped() { return &overlapped_; }

  // Implements base::win::ObjectWatcher::Delegate.
  virtual void OnObjectSignaled(HANDLE object) OVERRIDE;

  // Implements base::MessageLoop::DestructionObserver
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  scoped_refptr<HidConnectionWin> connection_;
  scoped_refptr<net::IOBufferWithSize> target_buffer_;
  scoped_refptr<net::IOBufferWithSize> receive_buffer_;
  HidConnection::IOCallback callback_;
  OVERLAPPED overlapped_;
  base::win::ScopedHandle event_;
  base::win::ObjectWatcher watcher_;

 private:
  friend class base::RefCounted<PendingHidTransfer>;

  virtual ~PendingHidTransfer();

  DISALLOW_COPY_AND_ASSIGN(PendingHidTransfer);
};

PendingHidTransfer::PendingHidTransfer(
    scoped_refptr<HidConnectionWin> connection,
    scoped_refptr<net::IOBufferWithSize> target_buffer,
    scoped_refptr<net::IOBufferWithSize> receive_buffer,
    HidConnection::IOCallback callback)
    : connection_(connection),
      target_buffer_(target_buffer),
      receive_buffer_(receive_buffer),
      callback_(callback),
      event_(CreateEvent(NULL, FALSE, FALSE, NULL)) {
  memset(&overlapped_, 0, sizeof(OVERLAPPED));
  overlapped_.hEvent = event_.Get();
}

PendingHidTransfer::~PendingHidTransfer() {
  base::MessageLoop::current()->RemoveDestructionObserver(this);
}

void PendingHidTransfer::TakeResultFromWindowsAPI(BOOL result) {
  if (result || GetLastError() != ERROR_IO_PENDING) {
    connection_->OnTransferFinished(this);
  } else {
    base::MessageLoop::current()->AddDestructionObserver(this);
    AddRef();
    watcher_.StartWatching(event_.Get(), this);
  }
}

void PendingHidTransfer::OnObjectSignaled(HANDLE event_handle) {
  connection_->OnTransferFinished(this);
  Release();
}

void PendingHidTransfer::WillDestroyCurrentMessageLoop() {
  watcher_.StopWatching();
  connection_->OnTransferCanceled(this);
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

void HidConnectionWin::PlatformRead(scoped_refptr<net::IOBufferWithSize> buffer,
                                    const HidConnection::IOCallback& callback) {
  int expected_report_size = device_info().max_input_report_size;
  if (device_info().has_report_id) {
    expected_report_size++;
  }
  scoped_refptr<net::IOBufferWithSize> receive_buffer =
      new net::IOBufferWithSize(expected_report_size);

  scoped_refptr<PendingHidTransfer> transfer(
      new PendingHidTransfer(this, buffer, receive_buffer, callback));
  transfers_.insert(transfer);
  transfer->TakeResultFromWindowsAPI(
      ReadFile(file_.Get(),
               receive_buffer->data(),
               static_cast<DWORD>(receive_buffer->size()),
               NULL,
               transfer->GetOverlapped()));
}

void HidConnectionWin::PlatformWrite(
    uint8_t report_id,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const HidConnection::IOCallback& callback) {
  // The Windows API always wants either a report ID (if supported) or
  // zero at the front of every output report.
  scoped_refptr<net::IOBufferWithSize> output_buffer(buffer);
  output_buffer = new net::IOBufferWithSize(buffer->size() + 1);
  output_buffer->data()[0] = report_id;
  memcpy(output_buffer->data() + 1, buffer->data(), buffer->size());

  scoped_refptr<PendingHidTransfer> transfer(
      new PendingHidTransfer(this, output_buffer, NULL, callback));
  transfers_.insert(transfer);
  transfer->TakeResultFromWindowsAPI(
      WriteFile(file_.Get(),
                output_buffer->data(),
                static_cast<DWORD>(output_buffer->size()),
                NULL,
                transfer->GetOverlapped()));
}

void HidConnectionWin::PlatformGetFeatureReport(
    uint8_t report_id,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const IOCallback& callback) {
  int expected_report_size = device_info().max_feature_report_size;
  if (device_info().has_report_id) {
    expected_report_size++;
  }
  scoped_refptr<net::IOBufferWithSize> receive_buffer =
      new net::IOBufferWithSize(expected_report_size);
  // The first byte of the destination buffer is the report ID being requested.
  receive_buffer->data()[0] = report_id;

  scoped_refptr<PendingHidTransfer> transfer(
      new PendingHidTransfer(this, buffer, receive_buffer, callback));
  transfers_.insert(transfer);
  transfer->TakeResultFromWindowsAPI(
      DeviceIoControl(file_.Get(),
                      IOCTL_HID_GET_FEATURE,
                      NULL,
                      0,
                      receive_buffer->data(),
                      static_cast<DWORD>(receive_buffer->size()),
                      NULL,
                      transfer->GetOverlapped()));
}

void HidConnectionWin::PlatformSendFeatureReport(
    uint8_t report_id,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const IOCallback& callback) {
  // The Windows API always wants either a report ID (if supported) or
  // zero at the front of every output report.
  scoped_refptr<net::IOBufferWithSize> output_buffer(buffer);
  output_buffer = new net::IOBufferWithSize(buffer->size() + 1);
  output_buffer->data()[0] = report_id;
  memcpy(output_buffer->data() + 1, buffer->data(), buffer->size());

  scoped_refptr<PendingHidTransfer> transfer(
      new PendingHidTransfer(this, output_buffer, NULL, callback));
  transfer->TakeResultFromWindowsAPI(
      DeviceIoControl(file_.Get(),
                      IOCTL_HID_SET_FEATURE,
                      output_buffer->data(),
                      static_cast<DWORD>(output_buffer->size()),
                      NULL,
                      0,
                      NULL,
                      transfer->GetOverlapped()));
}

void HidConnectionWin::OnTransferFinished(
    scoped_refptr<PendingHidTransfer> transfer) {
  transfers_.erase(transfer);

  DWORD bytes_transferred;
  if (GetOverlappedResult(
          file_, transfer->GetOverlapped(), &bytes_transferred, FALSE)) {
    if (bytes_transferred == 0) {
      transfer->callback_.Run(true, 0);
      return;
    }

    if (transfer->receive_buffer_) {
      // If owner HID top-level collection does not have report ID, we need to
      // copy the receive buffer into the target buffer, discarding the first
      // byte. This is because the target buffer's owner is not expecting a
      // report ID but Windows will always provide one.
      if (!device_info().has_report_id) {
        uint8_t report_id = transfer->receive_buffer_->data()[0];
        // Assert first byte is 0x00
        if (report_id != HidConnection::kNullReportId) {
          VLOG(1) << "Unexpected report ID in HID report:" << report_id;
          transfer->callback_.Run(false, 0);
        } else {
          // Move one byte forward.
          --bytes_transferred;
          memcpy(transfer->target_buffer_->data(),
                 transfer->receive_buffer_->data() + 1,
                 bytes_transferred);
        }
      } else {
        memcpy(transfer->target_buffer_->data(),
               transfer->receive_buffer_->data(),
               bytes_transferred);
      }
    }

    CompleteRead(transfer->target_buffer_, transfer->callback_);
  } else {
    transfer->callback_.Run(false, 0);
  }
}

void HidConnectionWin::OnTransferCanceled(
    scoped_refptr<PendingHidTransfer> transfer) {
  transfers_.erase(transfer);
  transfer->callback_.Run(false, 0);
}

}  // namespace device
