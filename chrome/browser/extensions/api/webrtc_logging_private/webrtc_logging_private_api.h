// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_WEBRTC_LOGGING_PRIVATE_WEBRTC_LOGGING_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_WEBRTC_LOGGING_PRIVATE_WEBRTC_LOGGING_PRIVATE_API_H_

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/api/webrtc_logging_private.h"

namespace extensions {

class WebrtcLoggingPrivateSetMetaDataFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.setMetaData",
                             WEBRTCLOGGINGPRIVATE_SETMETADATA)
  WebrtcLoggingPrivateSetMetaDataFunction();

 private:
  virtual ~WebrtcLoggingPrivateSetMetaDataFunction();

  // ExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // Must be called on UI thread.
  void SetMetaDataCallback(bool success, const std::string& error_message);
};

class WebrtcLoggingPrivateStartFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.start",
                             WEBRTCLOGGINGPRIVATE_START)
  WebrtcLoggingPrivateStartFunction();

 private:
  virtual ~WebrtcLoggingPrivateStartFunction();

  // ExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // Must be called on UI thread.
  void StartCallback(bool success, const std::string& error_message);
};

class WebrtcLoggingPrivateSetUploadOnRenderCloseFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.setUploadOnRenderClose",
                             WEBRTCLOGGINGPRIVATE_SETUPLOADONRENDERCLOSE)
  WebrtcLoggingPrivateSetUploadOnRenderCloseFunction();

 private:
  virtual ~WebrtcLoggingPrivateSetUploadOnRenderCloseFunction();

  // ExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

class WebrtcLoggingPrivateStopFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.stop",
                             WEBRTCLOGGINGPRIVATE_STOP)
  WebrtcLoggingPrivateStopFunction();

 private:
  virtual ~WebrtcLoggingPrivateStopFunction();

  // ExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // Must be called on UI thread.
  void StopCallback(bool success, const std::string& error_message);
};

class WebrtcLoggingPrivateUploadFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.upload",
                             WEBRTCLOGGINGPRIVATE_UPLOAD)
  WebrtcLoggingPrivateUploadFunction();

 private:
  virtual ~WebrtcLoggingPrivateUploadFunction();

  // ExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // Must be called on UI thread.
  void UploadCallback(bool success, const std::string& report_id,
                      const std::string& error_message);
};

class WebrtcLoggingPrivateDiscardFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("webrtcLoggingPrivate.discard",
                             WEBRTCLOGGINGPRIVATE_DISCARD)
  WebrtcLoggingPrivateDiscardFunction();

 private:
  virtual ~WebrtcLoggingPrivateDiscardFunction();

  // ExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

  // Must be called on UI thread.
  void DiscardCallback(bool success, const std::string& error_message);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_WEBRTC_LOGGING_PRIVATE_WEBRTC_LOGGING_PRIVATE_API_H_
