// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection_win.h"

#include <cstring>

#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread_restrictions.h"
#include "device/hid/hid_service.h"
#include "device/hid/hid_service_win.h"
#include "net/base/io_buffer.h"

#if defined(OS_WIN)

#define INITGUID

#include <windows.h>
#include <hidclass.h>

extern "C" {

#include <hidsdi.h>

}

#include <setupapi.h>
#include <winioctl.h>
#include "base/win/scoped_handle.h"

#endif  // defined(OS_WIN)

namespace device {

HidConnectionWin::PendingTransfer::PendingTransfer(
    scoped_refptr<HidConnectionWin> conn,
    scoped_refptr<net::IOBuffer> target,
    scoped_refptr<net::IOBuffer> receiving,
    bool is_input,
    IOCallback callback)
    : conn_(conn),
      is_input_(is_input),
      target_(target),
      receiving_(receiving),
      callback_(callback),
      event_(CreateEvent(NULL, FALSE, FALSE, NULL)) {
  memset(&overlapped_, 0, sizeof(OVERLAPPED));
  overlapped_.hEvent = event_.Get();
}
HidConnectionWin::PendingTransfer::~PendingTransfer() {
  base::MessageLoop::current()->RemoveDestructionObserver(this);
}

void HidConnectionWin::PendingTransfer::TakeResultFromWindowsAPI(BOOL result) {
  if (result || GetLastError() != ERROR_IO_PENDING) {
    conn_->OnTransferFinished(this);
  } else {
    base::MessageLoop::current()->AddDestructionObserver(this);
    AddRef();
    watcher_.StartWatching(event_.Get(), this);
  }
}

void HidConnectionWin::PendingTransfer::OnObjectSignaled(HANDLE event_handle) {
  conn_->OnTransferFinished(this);
  Release();
}

void HidConnectionWin::PendingTransfer::WillDestroyCurrentMessageLoop() {
  watcher_.StopWatching();
  conn_->OnTransferCanceled(this);
}

void HidConnectionWin::OnTransferFinished(
    scoped_refptr<PendingTransfer> transfer) {
  DWORD bytes_transfered;
  transfers_.erase(transfer);
  if (GetOverlappedResult(file_,
                          transfer->GetOverlapped(),
                          &bytes_transfered,
                          FALSE)) {
    if (transfer->is_input_ && !device_info_.has_report_id) {
      // Move one byte forward.
      --bytes_transfered;
      memcpy(transfer->target_->data(),
             transfer->receiving_->data() + 1,
             bytes_transfered);
    }
    transfer->callback_.Run(true, bytes_transfered);
  } else {
    transfer->callback_.Run(false, 0);
  }
}

void HidConnectionWin::OnTransferCanceled(
    scoped_refptr<PendingTransfer> transfer) {
  transfers_.erase(transfer);
  transfer->callback_.Run(false, 0);
}

HidConnectionWin::HidConnectionWin(HidDeviceInfo device_info)
    : HidConnection(device_info),
      available_(false) {
  DCHECK(thread_checker_.CalledOnValidThread());
  file_.Set(CreateFileA(device_info.device_id.c_str(),
                        GENERIC_WRITE | GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED,
                        NULL));
  available_ = file_.IsValid();
}

HidConnectionWin::~HidConnectionWin() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CancelIo(file_.Get());
}

void HidConnectionWin::Read(scoped_refptr<net::IOBuffer> buffer,
                            size_t size,
                            const HidConnection::IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  size_t report_size = device_info_.input_report_size;
  if (report_size == 0) {
    // The device does not supoort input reports.
    callback.Run(false, 0);
    return;
  }

  if (size + !device_info_.has_report_id < report_size) {
    // Buffer too short.
    callback.Run(false, 0);
    return;
  }

  scoped_refptr<net::IOBuffer> expanded_buffer;
  if (!device_info_.has_report_id) {
    ++size;
    expanded_buffer = new net::IOBuffer(static_cast<int>(size));
  }

  scoped_refptr<PendingTransfer> transfer(
      new PendingTransfer(this, buffer, expanded_buffer, true, callback));
  transfers_.insert(transfer);
  transfer->TakeResultFromWindowsAPI(ReadFile(file_.Get(),
                                              device_info_.has_report_id ?
                                                  buffer->data() :
                                                  expanded_buffer->data(),
                                              static_cast<DWORD>(size),
                                              NULL,
                                              transfer->GetOverlapped()));
}

void HidConnectionWin::Write(scoped_refptr<net::IOBuffer> buffer,
                             size_t size,
                             const HidConnection::IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  size_t report_size = device_info_.output_report_size;
  if (report_size == 0) {
    // The device does not supoort output reports.
    callback.Run(false, 0);
    return;
  }

  if (size + !device_info_.has_report_id > report_size) {
    // Size of report too long.
    callback.Run(false, 0);
    return;
  }

  scoped_refptr<net::IOBuffer> expanded_buffer;
  if (!device_info_.has_report_id) {
    expanded_buffer = new net::IOBuffer(
        static_cast<int>(device_info_.output_report_size));
    memset(expanded_buffer->data(), 0, device_info_.output_report_size);
    memcpy(expanded_buffer->data() + 1,
           buffer->data(),
           size);
    size++;
  }

  scoped_refptr<PendingTransfer> transfer(
      new PendingTransfer(this, buffer, expanded_buffer, false, callback));
  transfers_.insert(transfer);
  transfer->TakeResultFromWindowsAPI(
      WriteFile(file_.Get(),
                device_info_.has_report_id ?
                    buffer->data() : expanded_buffer->data(),
                static_cast<DWORD>(device_info_.output_report_size),
                NULL,
                transfer->GetOverlapped()));
}

void HidConnectionWin::GetFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                        size_t size,
                                        const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  size_t report_size = device_info_.feature_report_size;
  if (report_size == 0) {
    // The device does not supoort input reports.
    callback.Run(false, 0);
    return;
  }

  if (size + !device_info_.has_report_id < report_size) {
    // Buffer too short.
    callback.Run(false, 0);
    return;
  }

  scoped_refptr<net::IOBuffer> expanded_buffer;
  if (!device_info_.has_report_id) {
    ++size;
    expanded_buffer = new net::IOBuffer(static_cast<int>(size));
  }

  scoped_refptr<PendingTransfer> transfer(
      new PendingTransfer(this, buffer, expanded_buffer, true, callback));
  transfers_.insert(transfer);
  transfer->TakeResultFromWindowsAPI(
      DeviceIoControl(file_.Get(),
                      IOCTL_HID_GET_FEATURE,
                      NULL,
                      0,
                      device_info_.has_report_id ?
                          buffer->data() :
                          expanded_buffer->data(),
                      static_cast<DWORD>(size),
                      NULL,
                      transfer->GetOverlapped()));
}

void HidConnectionWin::SendFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                         size_t size,
                                         const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  size_t report_size = device_info_.feature_report_size;
  if (report_size == 0) {
    // The device does not supoort output reports.
    callback.Run(false, 0);
    return;
  }

  if (size + !device_info_.has_report_id > report_size) {
    // Size of report too long.
    callback.Run(false, 0);
    return;
  }

  scoped_refptr<net::IOBuffer> expanded_buffer;
  if (!device_info_.has_report_id) {
    expanded_buffer = new net::IOBuffer(
        static_cast<int>(device_info_.feature_report_size));
    memset(expanded_buffer->data(), 0, device_info_.feature_report_size);
    memcpy(expanded_buffer->data() + 1,
           buffer->data(),
           size);
    size++;
  }

  scoped_refptr<PendingTransfer> transfer(
      new PendingTransfer(this, buffer, expanded_buffer, false, callback));
  transfer->TakeResultFromWindowsAPI(
      DeviceIoControl(file_.Get(),
                      IOCTL_HID_SET_FEATURE,
                      device_info_.has_report_id ?
                          buffer->data() :
                          expanded_buffer->data(),
                      static_cast<DWORD>(device_info_.output_report_size),
                      NULL,
                      0,
                      NULL,
                      transfer->GetOverlapped()));
}

}  // namespace device
