// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_CONNECTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_CONNECTION_H_

#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"

using content::BrowserThread;

namespace extensions {

extern const char kSerialConnectionNotFoundError[];

class ApiResourceEventNotifier;

// Encapsulates an open serial port. Platform-specific implementations are in
// _win and _posix versions of the the .cc file.
class SerialConnection : public ApiResource {
 public:
  SerialConnection(const std::string& port,
                   int bitrate,
                   const std::string& owner_extension_id,
                   ApiResourceEventNotifier* event_notifier);
  virtual ~SerialConnection();

  virtual bool Open();
  virtual void Close();
  virtual void Flush();

  virtual int Read(scoped_refptr<net::IOBufferWithSize> io_buffer);
  virtual int Write(scoped_refptr<net::IOBuffer> io_buffer, int byte_count);

  struct ControlSignals {
    // Sent from workstation to device. The should_set_ values indicate whether
    // SetControlSignals should change the given signal (true) or else leave it
    // as-is (false).
    bool should_set_dtr;
    bool dtr;
    bool should_set_rts;
    bool rts;

    // Received by workstation from device. DCD (Data Carrier Detect) is
    // equivalent to RLSD (Receive Line Signal Detect) on some platforms.
    bool dcd;
    bool cts;
  };

  virtual bool GetControlSignals(ControlSignals &control_signals);
  virtual bool SetControlSignals(const ControlSignals &control_signals);

 protected:
  // Do platform-specific work after a successful Open().
  bool PostOpen();

 private:
  std::string port_;
  int bitrate_;
  base::PlatformFile file_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SERIAL_SERIAL_CONNECTION_H_
