// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/webcam_private/visca_webcam.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Message terminator:
const char VISCA_TERMINATOR = 0xFF;

// Response types:
const char VISCA_RESPONSE_NETWORK_CHANGE = 0x38;
const char VISCA_RESPONSE_ACK = 0x40;
const char VISCA_RESPONSE_ERROR = 0x60;

// Pan-Tilt-Zoom movement comands from http://www.manualslib.com/manual/...
// 557364/Cisco-Precisionhd-1080p12x.html?page=31#manual

// Reset the address of each device in the VISCA chain (broadcast). This is used
// when resetting the VISCA network.
const std::vector<char> kSetAddressCommand = {0x88, 0x30, 0x01, 0xFF};

// Clear all of the devices, halting any pending commands in the VISCA chain
// (broadcast). This is used when resetting the VISCA network.
const std::vector<char> kClearAllCommand = {0x88, 0x01, 0x00, 0x01, 0xFF};

// Clear the command buffer in the target device and cancel the command
// currently being executed. Command: {0x8X, 0x01, 0x00, 0x01, 0xFF}, X = 1 to
// 7: target device address.
const std::vector<char> kClearCommand = {0x81, 0x01, 0x00, 0x01, 0xFF};

// Command: {0x8X, 0x09, 0x06, 0x12, 0xFF}, X = 1 to 7: target device address.
// Response: {0xY0, 0x50, 0x0p, 0x0q, 0x0r, 0x0s, 0x0t, 0x0u, 0x0v, 0x0w, 0xFF},
// Y = socket number; pqrs: pan position; tuvw: tilt position.
const std::vector<char> kGetPanTiltCommand = {0x81, 0x09, 0x06, 0x12, 0xFF};

// Command: {0x8X, 0x01, 0x06, 0x20, 0x0p, 0x0t, 0x0q, 0x0r, 0x0s, 0x0u, 0x0v,
// 0x0w, 0x0y, 0x0z, 0xFF}, X = 1 to 7: target device address; p = pan speed;
// t = tilt speed; qrsu = pan position; vwyz = tilt position.
const std::vector<char> kSetPanTiltCommand = {0x81, 0x01, 0x06, 0x02, 0x05,
    0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF};

// Command: {0x8X, 0x01, 0x06, 0x05, 0xFF}, X = 1 to 7: target device address.
const std::vector<char> kResetPanTiltCommand = {0x81, 0x01, 0x06, 0x05, 0xFF};

// Command: {0x8X, 0x09, 0x04, 0x47, 0xFF}, X = 1 to 7: target device address.
// Response: {0xY0, 0x50, 0x0p, 0x0q, 0x0r, 0x0s, 0xFF}, Y = socket number;
// pqrs: zoom position.
const std::vector<char> kGetZoomCommand = {0x81, 0x09, 0x04, 0x47, 0xFF};

// Command: {0x8X, 0x01, 0x04, 0x47, 0x0p, 0x0q, 0x0r, 0x0s, 0xFF}, X = 1 to 7:
// target device address; pqrs: zoom position;
const std::vector<char> kSetZoomCommand =
    {0x81, 0x01, 0x04, 0x47, 0x00, 0x00, 0x00, 0x00, 0xFF};

// Command: {0x8X, 0x01, 0x06, 0x01, 0x0p, 0x0t, 0x03, 0x01, 0xFF}, X = 1 to 7:
// target device address; p: pan speed; t: tilt speed.
const std::vector<char> kPTUpCommand =
    {0x81, 0x01, 0x06, 0x01, 0x05, 0x05, 0x03, 0x01, 0xFF};

// Command: {0x8X, 0x01, 0x06, 0x01, 0x0p, 0x0t, 0x03, 0x02, 0xFF}, X = 1 to 7:
// target device address; p: pan speed; t: tilt speed.
const std::vector<char> kPTDownCommand =
    {0x81, 0x01, 0x06, 0x01, 0x05, 0x05, 0x03, 0x02, 0xFF};

// Command: {0x8X, 0x01, 0x06, 0x01, 0x0p, 0x0t, 0x0, 0x03, 0xFF}, X = 1 to 7:
// target device address; p: pan speed; t: tilt speed.
const std::vector<char> kPTLeftCommand =
    {0x81, 0x01, 0x06, 0x01, 0x05, 0x05, 0x01, 0x03, 0xFF};

// Command: {0x8X, 0x01, 0x06, 0x01, 0x0p, 0x0t, 0x02, 0x03, 0xFF}, X = 1 to 7:
// target device address; p: pan speed; t: tilt speed.
const std::vector<char> kPTRightCommand =
    {0x81, 0x01, 0x06, 0x01, 0x05, 0x05, 0x02, 0x03, 0xFF};

// Command: {0x8X, 0x01, 0x06, 0x01, 0x03, 0x03, 0x03, 0x03, 0xFF}, X = 1 to 7:
// target device address.
const std::vector<char> kPTStopCommand =
    {0x81, 0x01, 0x06, 0x01, 0x03, 0x03, 0x03, 0x03, 0xFF};

}  // namespace

namespace extensions {

ViscaWebcam::ViscaWebcam(const std::string& path,
                         const std::string& extension_id)
    : path_(path),
      extension_id_(extension_id),
      pan_(0),
      tilt_(0),
      weak_ptr_factory_(this) {
}

ViscaWebcam::~ViscaWebcam() {
}

void ViscaWebcam::Open(const OpenCompleteCallback& open_callback) {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ViscaWebcam::OpenOnIOThread, weak_ptr_factory_.GetWeakPtr(),
                 open_callback));
}

void ViscaWebcam::OpenOnIOThread(const OpenCompleteCallback& open_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  core_api::serial::ConnectionOptions options;

  // Set the receive buffer size to receive the response data 1 by 1.
  options.buffer_size.reset(new int(1));
  options.persistent.reset(new bool(false));
  options.bitrate.reset(new int(9600));
  options.cts_flow_control.reset(new bool(false));
  options.receive_timeout.reset(new int(0));
  options.send_timeout.reset(new int(0));
  options.data_bits = core_api::serial::DATA_BITS_EIGHT;
  options.parity_bit = core_api::serial::PARITY_BIT_NO;
  options.stop_bits = core_api::serial::STOP_BITS_ONE;

  serial_connection_.reset(new SerialConnection(path_, extension_id_));
  serial_connection_->Open(
      options, base::Bind(&ViscaWebcam::OnConnected,
                          weak_ptr_factory_.GetWeakPtr(), open_callback));
}

void ViscaWebcam::OnConnected(const OpenCompleteCallback& open_callback,
                              bool success) {
  if (!success) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(open_callback, false));
  } else {
    Send(kSetAddressCommand,
         base::Bind(&ViscaWebcam::OnAddressSetCompleted,
                    weak_ptr_factory_.GetWeakPtr(), open_callback));
  }
}

void ViscaWebcam::OnAddressSetCompleted(
    const OpenCompleteCallback& open_callback,
    bool success,
    const std::vector<char>& response) {
  if (!success) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(open_callback, false));
  } else {
    Send(kClearAllCommand,
         base::Bind(&ViscaWebcam::OnClearAllCompleted,
                    weak_ptr_factory_.GetWeakPtr(), open_callback));
  }
}

void ViscaWebcam::OnClearAllCompleted(const OpenCompleteCallback& open_callback,
                                      bool success,
                                      const std::vector<char>& response) {
  if (!success) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(open_callback, false));
  } else {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(open_callback, true));
    // Get the current pan and tilt position to initilize |pan| and |tilt|.
    int value;
    Send(kGetPanTiltCommand,
         base::Bind(&ViscaWebcam::OnCommandCompleted,
                    weak_ptr_factory_.GetWeakPtr(), INQUIRY_PAN_TILT, &value));
  }
}

void ViscaWebcam::Send(const std::vector<char>& command,
                       const CommandCompleteCallback& callback) {
  if (!commands_.empty() ||
      !serial_connection_->Send(
          command, base::Bind(&ViscaWebcam::OnSendCompleted,
                              weak_ptr_factory_.GetWeakPtr(), callback))) {
    commands_.push_back(std::make_pair(command, callback));
  }
}

void ViscaWebcam::OnSendCompleted(const CommandCompleteCallback& callback,
                                  int bytes_sent,
                                  core_api::serial::SendError error) {
  if (error == core_api::serial::SEND_ERROR_NONE) {
    ReceiveLoop(callback);
  } else {
    const std::vector<char> response;
    callback.Run(false, response);
  }
}

void ViscaWebcam::ReceiveLoop(const CommandCompleteCallback& callback) {
  serial_connection_->Receive(base::Bind(&ViscaWebcam::OnReceiveCompleted,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         callback));
}

void ViscaWebcam::OnReceiveCompleted(const CommandCompleteCallback& callback,
                                     const std::vector<char>& data,
                                     core_api::serial::ReceiveError error) {
  data_buffer_.insert(data_buffer_.end(), data.begin(), data.end());

  if (error == core_api::serial::RECEIVE_ERROR_NONE) {
    // Loop until encounter the terminator.
    if (int(data_buffer_.back()) != VISCA_TERMINATOR) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&ViscaWebcam::ReceiveLoop,
                                weak_ptr_factory_.GetWeakPtr(), callback));
    } else {
      // Clear |data_buffer_|.
      std::vector<char> response;
      response.swap(data_buffer_);

      if ((int(response[1]) & 0xF0) == VISCA_RESPONSE_ERROR) {
        callback.Run(false, response);
      } else if ((int(response[1]) & 0xF0) != VISCA_RESPONSE_ACK &&
                 (int(response[1]) & 0xFF) != VISCA_RESPONSE_NETWORK_CHANGE) {
        callback.Run(true, response);
      } else {
        base::MessageLoop::current()->PostTask(
            FROM_HERE, base::Bind(&ViscaWebcam::ReceiveLoop,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
      }
    }
  } else {
    // Clear |data_buffer_|.
    std::vector<char> response;
    response.swap(data_buffer_);
    callback.Run(false, response);
  }
}

void ViscaWebcam::OnCommandCompleted(CommandType type,
                                     int* value,
                                     bool success,
                                     const std::vector<char>& response) {
  // TODO(xdai): Error handling according to |response|.
  if (!success)
    return;

  switch (type) {
    case COMMAND:
      break;
    case INQUIRY_PAN:
      pan_ = ((int(response[2]) & 0x0F) << 12) +
             ((int(response[3]) & 0x0F) << 8) +
             ((int(response[4]) & 0x0F) << 4) + (int(response[5]) & 0x0F);
      *value = pan_;
      break;
    case INQUIRY_TILT:
      tilt_ = ((int(response[6]) & 0x0F) << 12) +
              ((int(response[7]) & 0x0F) << 8) +
              ((int(response[8]) & 0x0F) << 4) + (int(response[9]) & 0x0F);
      *value = tilt_;
      break;
    case INQUIRY_PAN_TILT:
      pan_ = ((int(response[2]) & 0x0F) << 12) +
             ((int(response[3]) & 0x0F) << 8) +
             ((int(response[4]) & 0x0F) << 4) + (int(response[5]) & 0x0F);
      tilt_ = ((int(response[6]) & 0x0F) << 12) +
              ((int(response[7]) & 0x0F) << 8) +
              ((int(response[8]) & 0x0F) << 4) + (int(response[9]) & 0x0F);
      break;
    case INQUIRY_ZOOM:
      *value = ((int(response[2]) & 0x0F) << 12) +
               ((int(response[3]) & 0x0F) << 8) +
               ((int(response[4]) & 0x0F) << 4) + (int(response[5]) & 0x0F);
      break;
  }

  // If there are pending commands, process the next one.
  if (!commands_.empty()) {
    const std::vector<char> command = commands_.front().first;
    const CommandCompleteCallback callback = commands_.front().second;
    commands_.pop_front();
    serial_connection_->Send(
        command, base::Bind(&ViscaWebcam::OnSendCompleted,
                            weak_ptr_factory_.GetWeakPtr(), callback));
  }
}

void ViscaWebcam::Reset(bool pan, bool tilt, bool zoom) {
  int value;
  // pan and tilt are always reset together in Visca Webcams.
  if (pan || tilt) {
    Send(kResetPanTiltCommand,
         base::Bind(&ViscaWebcam::OnCommandCompleted,
                    weak_ptr_factory_.GetWeakPtr(), COMMAND, &value));
  }
  if (zoom) {
    const int default_zoom = 1;
    SetZoom(default_zoom);
  }
}

bool ViscaWebcam::GetPan(int* value) {
  Send(kGetPanTiltCommand,
       base::Bind(&ViscaWebcam::OnCommandCompleted,
                  weak_ptr_factory_.GetWeakPtr(), INQUIRY_PAN, value));
  return true;
}

bool ViscaWebcam::GetTilt(int* value) {
  Send(kGetPanTiltCommand,
       base::Bind(&ViscaWebcam::OnCommandCompleted,
                  weak_ptr_factory_.GetWeakPtr(), INQUIRY_TILT, value));
  return true;
}

bool ViscaWebcam::GetZoom(int* value) {
  Send(kGetZoomCommand,
       base::Bind(&ViscaWebcam::OnCommandCompleted,
                  weak_ptr_factory_.GetWeakPtr(), INQUIRY_ZOOM, value));
  return true;
}

bool ViscaWebcam::SetPan(int value) {
  pan_ = value;
  std::vector<char> command = kSetPanTiltCommand;
  command[6] |= ((pan_ & 0xF000) >> 12);
  command[7] |= ((pan_ & 0x0F00) >> 8);
  command[8] |= ((pan_ & 0x00F0) >> 4);
  command[9] |= (pan_ & 0x000F);
  command[10] |= ((tilt_ & 0xF000) >> 12);
  command[11] |= ((tilt_ & 0x0F00) >> 8);
  command[12] |= ((tilt_ & 0x00F0) >> 4);
  command[13] |= (tilt_ & 0x000F);
  Send(command, base::Bind(&ViscaWebcam::OnCommandCompleted,
                           weak_ptr_factory_.GetWeakPtr(), COMMAND, &value));
  return true;
}

bool ViscaWebcam::SetTilt(int value) {
  tilt_ = value;
  std::vector<char> command = kSetPanTiltCommand;
  command[6] |= ((pan_ & 0xF000) >> 12);
  command[7] |= ((pan_ & 0x0F00) >> 8);
  command[8] |= ((pan_ & 0x00F0) >> 4);
  command[9] |= (pan_ & 0x000F);
  command[10] |= ((tilt_ & 0xF000) >> 12);
  command[11] |= ((tilt_ & 0x0F00) >> 8);
  command[12] |= ((tilt_ & 0x00F0) >> 4);
  command[13] |= (tilt_ & 0x000F);
  Send(command, base::Bind(&ViscaWebcam::OnCommandCompleted,
                           weak_ptr_factory_.GetWeakPtr(), COMMAND, &value));
  return true;
}

bool ViscaWebcam::SetZoom(int value) {
  std::vector<char> command = kSetZoomCommand;
  command[4] |= ((value & 0xF000) >> 12);
  command[5] |= ((value & 0x0F00) >> 8);
  command[6] |= ((value & 0x00F0) >> 4);
  command[7] |= (value & 0x000F);
  Send(command, base::Bind(&ViscaWebcam::OnCommandCompleted,
                           weak_ptr_factory_.GetWeakPtr(), COMMAND, &value));
  return true;
}

bool ViscaWebcam::SetPanDirection(PanDirection direction) {
  std::vector<char> command = kPTStopCommand;
  switch (direction) {
    case PAN_STOP:
      break;
    case PAN_RIGHT:
      command = kPTRightCommand;
      break;
    case PAN_LEFT:
      command = kPTLeftCommand;
      break;
  }
  Send(command, base::Bind(&ViscaWebcam::OnCommandCompleted,
                           weak_ptr_factory_.GetWeakPtr(), COMMAND, &pan_));
  return true;
}

bool ViscaWebcam::SetTiltDirection(TiltDirection direction) {
  std::vector<char> command = kPTStopCommand;
  switch (direction) {
    case TILT_STOP:
      break;
    case TILT_UP:
      command = kPTUpCommand;
      break;
    case TILT_DOWN:
      command = kPTDownCommand;
      break;
  }
  Send(command, base::Bind(&ViscaWebcam::OnCommandCompleted,
                           weak_ptr_factory_.GetWeakPtr(), COMMAND, &tilt_));
  return true;
}

}  // namespace extensions
