// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_CLIPBOARD_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_CLIPBOARD_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

// Writes data to the clipboard. Data format and buffer data will be written to
// are specified by parameters passed from the extension.
// Currently, only writing to standard buffer is possible.
class WriteDataClipboardFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  // Writes plain text to the buffer. Ignores url argument if sent from the
  // extension. Only standard buffer is currently supported.
  bool WritePlainText(const std::string& buffer);
  // Writes html to the buffer. Only standard buffer is currently supported.
  bool WriteHtml(const std::string& buffer);

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.clipboard.writeData")
};

// Reads data from the clipboard. Data format and buffer data will be written to
// are specified by parameters passed from the extension.
class ReadDataClipboardFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;

 private:
  // Reads plain text from the clipboard buffer. Callback will have only data
  // property set. Selection buffer not supported for Win and Mac.
  bool ReadPlainText(const std::string& buffer);
  // Reads html from the buffer. Selection buffer not supported for Win and Mac.
  bool ReadHtml(const std::string& buffer);

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.clipboard.readData")
};

// For a given clipbord buffer, returns list of data types available on it.
class GetAvailableMimeTypesClipboardFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl() OVERRIDE;
 private:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.clipboard.getAvailableTypes")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_CLIPBOARD_API_H_
