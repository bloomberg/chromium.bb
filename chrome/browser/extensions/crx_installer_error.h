// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_ERROR_H_
#define CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_ERROR_H_
#pragma once

#include "base/string16.h"

// Simple error class for CrxInstaller.
class CrxInstallerError {
 public:
  // Typed errors that need to be handled specially by clients.
  enum Type {
    ERROR_NONE,
    ERROR_OFF_STORE,
    ERROR_OTHER
  };

  CrxInstallerError() : type_(ERROR_NONE) {
  }

  explicit CrxInstallerError(const string16& message)
      : type_(message.empty() ? ERROR_NONE : ERROR_OTHER),
        message_(message) {
  }

  CrxInstallerError(Type type, const string16& message)
      : type_(type), message_(message) {
  }

  Type type() const { return type_; }
  const string16& message() const { return message_; }

 private:
  Type type_;
  string16 message_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_CRX_INSTALLER_ERROR_H_
