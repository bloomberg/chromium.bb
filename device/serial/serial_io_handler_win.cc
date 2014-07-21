// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "device/serial/serial_io_handler_win.h"

namespace device {

namespace {

int BitrateToSpeedConstant(int bitrate) {
#define BITRATE_TO_SPEED_CASE(x) \
  case x:                        \
    return CBR_##x;
  switch (bitrate) {
    BITRATE_TO_SPEED_CASE(110);
    BITRATE_TO_SPEED_CASE(300);
    BITRATE_TO_SPEED_CASE(600);
    BITRATE_TO_SPEED_CASE(1200);
    BITRATE_TO_SPEED_CASE(2400);
    BITRATE_TO_SPEED_CASE(4800);
    BITRATE_TO_SPEED_CASE(9600);
    BITRATE_TO_SPEED_CASE(14400);
    BITRATE_TO_SPEED_CASE(19200);
    BITRATE_TO_SPEED_CASE(38400);
    BITRATE_TO_SPEED_CASE(57600);
    BITRATE_TO_SPEED_CASE(115200);
    BITRATE_TO_SPEED_CASE(128000);
    BITRATE_TO_SPEED_CASE(256000);
    default:
      // If the bitrate doesn't match that of one of the standard
      // index constants, it may be provided as-is to the DCB
      // structure, according to MSDN.
      return bitrate;
  }
#undef BITRATE_TO_SPEED_CASE
}

int DataBitsEnumToConstant(serial::DataBits data_bits) {
  switch (data_bits) {
    case serial::DATA_BITS_SEVEN:
      return 7;
    case serial::DATA_BITS_EIGHT:
    default:
      return 8;
  }
}

int ParityBitEnumToConstant(serial::ParityBit parity_bit) {
  switch (parity_bit) {
    case serial::PARITY_BIT_EVEN:
      return EVENPARITY;
    case serial::PARITY_BIT_ODD:
      return ODDPARITY;
    case serial::PARITY_BIT_NO:
    default:
      return NOPARITY;
  }
}

int StopBitsEnumToConstant(serial::StopBits stop_bits) {
  switch (stop_bits) {
    case serial::STOP_BITS_TWO:
      return TWOSTOPBITS;
    case serial::STOP_BITS_ONE:
    default:
      return ONESTOPBIT;
  }
}

int SpeedConstantToBitrate(int speed) {
#define SPEED_TO_BITRATE_CASE(x) \
  case CBR_##x:                  \
    return x;
  switch (speed) {
    SPEED_TO_BITRATE_CASE(110);
    SPEED_TO_BITRATE_CASE(300);
    SPEED_TO_BITRATE_CASE(600);
    SPEED_TO_BITRATE_CASE(1200);
    SPEED_TO_BITRATE_CASE(2400);
    SPEED_TO_BITRATE_CASE(4800);
    SPEED_TO_BITRATE_CASE(9600);
    SPEED_TO_BITRATE_CASE(14400);
    SPEED_TO_BITRATE_CASE(19200);
    SPEED_TO_BITRATE_CASE(38400);
    SPEED_TO_BITRATE_CASE(57600);
    SPEED_TO_BITRATE_CASE(115200);
    SPEED_TO_BITRATE_CASE(128000);
    SPEED_TO_BITRATE_CASE(256000);
    default:
      // If it's not one of the standard index constants,
      // it should be an integral baud rate, according to
      // MSDN.
      return speed;
  }
#undef SPEED_TO_BITRATE_CASE
}

serial::DataBits DataBitsConstantToEnum(int data_bits) {
  switch (data_bits) {
    case 7:
      return serial::DATA_BITS_SEVEN;
    case 8:
    default:
      return serial::DATA_BITS_EIGHT;
  }
}

serial::ParityBit ParityBitConstantToEnum(int parity_bit) {
  switch (parity_bit) {
    case EVENPARITY:
      return serial::PARITY_BIT_EVEN;
    case ODDPARITY:
      return serial::PARITY_BIT_ODD;
    case NOPARITY:
    default:
      return serial::PARITY_BIT_NO;
  }
}

serial::StopBits StopBitsConstantToEnum(int stop_bits) {
  switch (stop_bits) {
    case TWOSTOPBITS:
      return serial::STOP_BITS_TWO;
    case ONESTOPBIT:
    default:
      return serial::STOP_BITS_ONE;
  }
}

}  // namespace

// static
scoped_refptr<SerialIoHandler> SerialIoHandler::Create(
    scoped_refptr<base::MessageLoopProxy> file_thread_message_loop) {
  return new SerialIoHandlerWin(file_thread_message_loop);
}

bool SerialIoHandlerWin::PostOpen() {
  DCHECK(!comm_context_);
  DCHECK(!read_context_);
  DCHECK(!write_context_);

  base::MessageLoopForIO::current()->RegisterIOHandler(file().GetPlatformFile(),
                                                       this);

  comm_context_.reset(new base::MessageLoopForIO::IOContext());
  comm_context_->handler = this;
  memset(&comm_context_->overlapped, 0, sizeof(comm_context_->overlapped));

  read_context_.reset(new base::MessageLoopForIO::IOContext());
  read_context_->handler = this;
  memset(&read_context_->overlapped, 0, sizeof(read_context_->overlapped));

  write_context_.reset(new base::MessageLoopForIO::IOContext());
  write_context_->handler = this;
  memset(&write_context_->overlapped, 0, sizeof(write_context_->overlapped));

  // A ReadIntervalTimeout of MAXDWORD will cause async reads to complete
  // immediately with any data that's available, even if there is none.
  // This is OK because we never issue a read request until WaitCommEvent
  // signals that data is available.
  COMMTIMEOUTS timeouts = {0};
  timeouts.ReadIntervalTimeout = MAXDWORD;
  if (!::SetCommTimeouts(file().GetPlatformFile(), &timeouts)) {
    return false;
  }

  DCB config = {0};
  config.DCBlength = sizeof(config);
  if (!GetCommState(file().GetPlatformFile(), &config)) {
    return false;
  }
  // Setup some sane default state.
  config.fBinary = TRUE;
  config.fParity = FALSE;
  config.fAbortOnError = TRUE;
  config.fOutxCtsFlow = FALSE;
  config.fOutxDsrFlow = FALSE;
  config.fRtsControl = RTS_CONTROL_ENABLE;
  config.fDtrControl = DTR_CONTROL_ENABLE;
  config.fDsrSensitivity = FALSE;
  config.fOutX = FALSE;
  config.fInX = FALSE;
  return SetCommState(file().GetPlatformFile(), &config) != 0;
}

void SerialIoHandlerWin::ReadImpl() {
  DCHECK(CalledOnValidThread());
  DCHECK(pending_read_buffer());
  DCHECK(file().IsValid());

  DWORD errors;
  COMSTAT status;
  if (!ClearCommError(file().GetPlatformFile(), &errors, &status) ||
      errors != 0) {
    QueueReadCompleted(0, serial::RECEIVE_ERROR_SYSTEM_ERROR);
    return;
  }

  SetCommMask(file().GetPlatformFile(), EV_RXCHAR);

  event_mask_ = 0;
  BOOL ok = ::WaitCommEvent(
      file().GetPlatformFile(), &event_mask_, &comm_context_->overlapped);
  if (!ok && GetLastError() != ERROR_IO_PENDING) {
    QueueReadCompleted(0, serial::RECEIVE_ERROR_SYSTEM_ERROR);
  }
  is_comm_pending_ = true;
}

void SerialIoHandlerWin::WriteImpl() {
  DCHECK(CalledOnValidThread());
  DCHECK(pending_write_buffer());
  DCHECK(file().IsValid());

  BOOL ok = ::WriteFile(file().GetPlatformFile(),
                        pending_write_buffer()->data(),
                        pending_write_buffer_len(),
                        NULL,
                        &write_context_->overlapped);
  if (!ok && GetLastError() != ERROR_IO_PENDING) {
    QueueWriteCompleted(0, serial::SEND_ERROR_SYSTEM_ERROR);
  }
}

void SerialIoHandlerWin::CancelReadImpl() {
  DCHECK(CalledOnValidThread());
  DCHECK(file().IsValid());
  ::CancelIo(file().GetPlatformFile());
}

void SerialIoHandlerWin::CancelWriteImpl() {
  DCHECK(CalledOnValidThread());
  DCHECK(file().IsValid());
  ::CancelIo(file().GetPlatformFile());
}

SerialIoHandlerWin::SerialIoHandlerWin(
    scoped_refptr<base::MessageLoopProxy> file_thread_message_loop)
    : SerialIoHandler(file_thread_message_loop),
      event_mask_(0),
      is_comm_pending_(false) {
}

SerialIoHandlerWin::~SerialIoHandlerWin() {
}

void SerialIoHandlerWin::OnIOCompleted(
    base::MessageLoopForIO::IOContext* context,
    DWORD bytes_transferred,
    DWORD error) {
  DCHECK(CalledOnValidThread());
  if (context == comm_context_) {
    if (read_canceled()) {
      ReadCompleted(bytes_transferred, read_cancel_reason());
    } else if (error != ERROR_SUCCESS && error != ERROR_OPERATION_ABORTED) {
      ReadCompleted(0, serial::RECEIVE_ERROR_SYSTEM_ERROR);
    } else if (pending_read_buffer()) {
      BOOL ok = ::ReadFile(file().GetPlatformFile(),
                           pending_read_buffer()->data(),
                           pending_read_buffer_len(),
                           NULL,
                           &read_context_->overlapped);
      if (!ok && GetLastError() != ERROR_IO_PENDING) {
        ReadCompleted(0, serial::RECEIVE_ERROR_SYSTEM_ERROR);
      }
    }
  } else if (context == read_context_) {
    if (read_canceled()) {
      ReadCompleted(bytes_transferred, read_cancel_reason());
    } else if (error != ERROR_SUCCESS && error != ERROR_OPERATION_ABORTED) {
      ReadCompleted(0, serial::RECEIVE_ERROR_SYSTEM_ERROR);
    } else {
      ReadCompleted(bytes_transferred,
                    error == ERROR_SUCCESS
                        ? serial::RECEIVE_ERROR_NONE
                        : serial::RECEIVE_ERROR_SYSTEM_ERROR);
    }
  } else if (context == write_context_) {
    DCHECK(pending_write_buffer());
    if (write_canceled()) {
      WriteCompleted(0, write_cancel_reason());
    } else if (error != ERROR_SUCCESS && error != ERROR_OPERATION_ABORTED) {
      WriteCompleted(0, serial::SEND_ERROR_SYSTEM_ERROR);
    } else {
      WriteCompleted(bytes_transferred,
                     error == ERROR_SUCCESS ? serial::SEND_ERROR_NONE
                                            : serial::SEND_ERROR_SYSTEM_ERROR);
    }
  } else {
    NOTREACHED() << "Invalid IOContext";
  }
}

bool SerialIoHandlerWin::ConfigurePort(
    const serial::ConnectionOptions& options) {
  DCB config = {0};
  config.DCBlength = sizeof(config);
  if (!GetCommState(file().GetPlatformFile(), &config)) {
    return false;
  }
  if (options.bitrate)
    config.BaudRate = BitrateToSpeedConstant(options.bitrate);
  if (options.data_bits != serial::DATA_BITS_NONE)
    config.ByteSize = DataBitsEnumToConstant(options.data_bits);
  if (options.parity_bit != serial::PARITY_BIT_NONE)
    config.Parity = ParityBitEnumToConstant(options.parity_bit);
  if (options.stop_bits != serial::STOP_BITS_NONE)
    config.StopBits = StopBitsEnumToConstant(options.stop_bits);
  if (options.has_cts_flow_control) {
    if (options.cts_flow_control) {
      config.fOutxCtsFlow = TRUE;
      config.fRtsControl = RTS_CONTROL_HANDSHAKE;
    } else {
      config.fOutxCtsFlow = FALSE;
      config.fRtsControl = RTS_CONTROL_ENABLE;
    }
  }
  return SetCommState(file().GetPlatformFile(), &config) != 0;
}

bool SerialIoHandlerWin::Flush() const {
  return PurgeComm(file().GetPlatformFile(), PURGE_RXCLEAR | PURGE_TXCLEAR) !=
         0;
}

serial::DeviceControlSignalsPtr SerialIoHandlerWin::GetControlSignals() const {
  DWORD status;
  if (!GetCommModemStatus(file().GetPlatformFile(), &status)) {
    return serial::DeviceControlSignalsPtr();
  }

  serial::DeviceControlSignalsPtr signals(serial::DeviceControlSignals::New());
  signals->dcd = (status & MS_RLSD_ON) != 0;
  signals->cts = (status & MS_CTS_ON) != 0;
  signals->dsr = (status & MS_DSR_ON) != 0;
  signals->ri = (status & MS_RING_ON) != 0;
  return signals.Pass();
}

bool SerialIoHandlerWin::SetControlSignals(
    const serial::HostControlSignals& signals) {
  if (signals.has_dtr) {
    if (!EscapeCommFunction(file().GetPlatformFile(),
                            signals.dtr ? SETDTR : CLRDTR)) {
      return false;
    }
  }
  if (signals.has_rts) {
    if (!EscapeCommFunction(file().GetPlatformFile(),
                            signals.rts ? SETRTS : CLRRTS)) {
      return false;
    }
  }
  return true;
}

serial::ConnectionInfoPtr SerialIoHandlerWin::GetPortInfo() const {
  DCB config = {0};
  config.DCBlength = sizeof(config);
  if (!GetCommState(file().GetPlatformFile(), &config)) {
    return serial::ConnectionInfoPtr();
  }
  serial::ConnectionInfoPtr info(serial::ConnectionInfo::New());
  info->bitrate = SpeedConstantToBitrate(config.BaudRate);
  info->data_bits = DataBitsConstantToEnum(config.ByteSize);
  info->parity_bit = ParityBitConstantToEnum(config.Parity);
  info->stop_bits = StopBitsConstantToEnum(config.StopBits);
  info->cts_flow_control = config.fOutxCtsFlow != 0;
  return info.Pass();
}

std::string SerialIoHandler::MaybeFixUpPortName(const std::string& port_name) {
  // For COM numbers less than 9, CreateFile is called with a string such as
  // "COM1". For numbers greater than 9, a prefix of "\\\\.\\" must be added.
  if (port_name.length() > std::string("COM9").length())
    return std::string("\\\\.\\").append(port_name);

  return port_name;
}

}  // namespace device
