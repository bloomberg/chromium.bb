// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMAGE_WRITER_IMAGE_WRITER_HANDLER_H_
#define CHROME_UTILITY_IMAGE_WRITER_IMAGE_WRITER_HANDLER_H_

#include <string>

#include "chrome/utility/image_writer/image_writer.h"
#include "chrome/utility/utility_message_handler.h"
#include "ipc/ipc_message.h"

namespace base {
class FilePath;
}

namespace image_writer {

// A handler for messages related to writing images.  This class is added as a
// handler in ChromeContentUtilityClient.
class ImageWriterHandler : public UtilityMessageHandler {
 public:
  ImageWriterHandler();
  virtual ~ImageWriterHandler();

  // Methods for sending the different messages back to the browser process.
  // Generally should be called by chrome::image_writer::ImageWriter.
  virtual void SendSucceeded();
  virtual void SendCancelled();
  virtual void SendFailed(const std::string& message);
  virtual void SendProgress(int64 progress);

 private:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Small wrapper for sending on the UtilityProcess.
  void Send(IPC::Message* msg);

  // Message handlers.
  void OnWriteStart(const base::FilePath& image, const base::FilePath& device);
  void OnVerifyStart(const base::FilePath& image, const base::FilePath& device);
  void OnCancel();

  scoped_ptr<ImageWriter> image_writer_;
};

}  // namespace image_writer

#endif  // CHROME_UTILITY_IMAGE_WRITER_IMAGE_WRITER_HANDLER_H_
