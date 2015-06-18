// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_VISCA_WEBCAM_H_
#define EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_VISCA_WEBCAM_H_

#include <deque>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/api/serial/serial_connection.h"
#include "extensions/browser/api/webcam_private/webcam.h"
#include "extensions/common/api/serial.h"

namespace extensions {

class ViscaWebcam : public Webcam {
 public:
  ViscaWebcam(const std::string& path, const std::string& extension_id);

  using OpenCompleteCallback = base::Callback<void(bool)>;

  // Open and initialize the web camera. This is done by the following three
  // steps (in order): 1. Open the serial port;  2. Request address; 3. Clear
  // the command buffer. After these three steps completes, |open_callback| will
  // be called.
  void Open(const OpenCompleteCallback& open_callback);

 private:
  ~ViscaWebcam() override;

  enum CommandType {
    COMMAND,
    INQUIRY_PAN,
    INQUIRY_TILT,
    INQUIRY_PAN_TILT,
    INQUIRY_ZOOM,
  };

  using CommandCompleteCallback =
      base::Callback<void(bool, const std::vector<char>&)>;

  void OpenOnIOThread(const OpenCompleteCallback& open_callback);
  // Callback function that will be called after the serial connection has been
  // opened successfully.
  void OnConnected(const OpenCompleteCallback& open_callback, bool success);
  // Callback function that will be called after the address has been set
  // successfully.
  void OnAddressSetCompleted(const OpenCompleteCallback& open_callback,
                             bool success,
                             const std::vector<char>& response);
  // Callback function that will be called after the command buffer has been
  // cleared successfully.
  void OnClearAllCompleted(const OpenCompleteCallback& open_callback,
                           bool success,
                           const std::vector<char>& response);

  // Send or queue a command and wait for the camera's response.
  void Send(const std::vector<char>& command,
            const CommandCompleteCallback& callback);
  void OnSendCompleted(const CommandCompleteCallback& callback,
                       int bytes_sent,
                       core_api::serial::SendError error);
  void ReceiveLoop(const CommandCompleteCallback& callback);
  void OnReceiveCompleted(const CommandCompleteCallback& callback,
                          const std::vector<char>& data,
                          core_api::serial::ReceiveError error);
  // Callback function that will be called after the send and reply of a command
  // are both completed. Update |value| according to |type| and |response| if
  // necessory.
  void OnCommandCompleted(CommandType type,
                          int* value,
                          bool success,
                          const std::vector<char>& response);

  // Webcam Overrides:
  void Reset(bool pan, bool tilt, bool zoom) override;
  bool GetPan(int* value) override;
  bool GetTilt(int* value) override;
  bool GetZoom(int* value) override;
  bool SetPan(int value) override;
  bool SetTilt(int value) override;
  bool SetZoom(int value) override;
  bool SetPanDirection(PanDirection direction) override;
  bool SetTiltDirection(TiltDirection direction) override;

  const std::string path_;
  const std::string extension_id_;

  scoped_ptr<SerialConnection> serial_connection_;

  // Stores the response for the current command.
  std::vector<char> data_buffer_;
  // Queues commands till the current command completes.
  std::deque<std::pair<std::vector<char>, CommandCompleteCallback>> commands_;

  // Visca webcam always get/set pan-tilt together. |pan| and |tilt| are used to
  // store the current value of pan and tilt positions.
  int pan_;
  int tilt_;

  base::WeakPtrFactory<ViscaWebcam> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ViscaWebcam);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_VISCA_WEBCAM_H_
