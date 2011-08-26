// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Clear API functions, which entail
// clearing browsing data, and clearing the browser's cache (which, let's be
// honest, are the same thing), as specified in
// chrome/common/extensions/api/extension_api.json.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_CLEAR_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_CLEAR_API_H_
#pragma once

#include <string>

#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/extensions/extension_function.h"

namespace base {
class DictionaryValue;
}

// This serves as a base class from which the browsing data API functions will
// inherit. Each needs to be an observer of BrowsingDataRemover events, and each
// will handle those events in the same way (by calling the passed-in callback
// function).
//
// Each child class must implement GetRemovalMask(), which returns the bitmask
// of data types to remove.
class BrowsingDataExtensionFunction : public AsyncExtensionFunction,
                                      public BrowsingDataRemover::Observer {
 public:
  // BrowsingDataRemover::Observer interface method.
  virtual void OnBrowsingDataRemoverDone() OVERRIDE;

  // AsyncExtensionFunction interface method.
  virtual bool RunImpl() OVERRIDE;

 protected:
  // Children should override this method to provide the proper removal mask
  // based on the API call they represent.
  virtual int GetRemovalMask() const = 0;
};

class ClearBrowsingDataFunction : public BrowsingDataExtensionFunction {
 public:
  ClearBrowsingDataFunction() {}
  virtual ~ClearBrowsingDataFunction() {}

 protected:
  // BrowsingDataExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.clear.browsingData")
};

class ClearCacheFunction : public BrowsingDataExtensionFunction {
 public:
  ClearCacheFunction() {}
  virtual ~ClearCacheFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.clear.cache")
};

class ClearCookiesFunction : public BrowsingDataExtensionFunction {
 public:
  ClearCookiesFunction() {}
  virtual ~ClearCookiesFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.clear.cookies")
};

class ClearDownloadsFunction : public BrowsingDataExtensionFunction {
 public:
  ClearDownloadsFunction() {}
  virtual ~ClearDownloadsFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.clear.downloads")
};

class ClearFormDataFunction : public BrowsingDataExtensionFunction {
 public:
  ClearFormDataFunction() {}
  virtual ~ClearFormDataFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.clear.formData")
};

class ClearHistoryFunction : public BrowsingDataExtensionFunction {
 public:
  ClearHistoryFunction() {}
  virtual ~ClearHistoryFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.clear.history")
};

class ClearPasswordsFunction : public BrowsingDataExtensionFunction {
 public:
  ClearPasswordsFunction() {}
  virtual ~ClearPasswordsFunction() {}

 protected:
  // BrowsingDataTypeExtensionFunction interface method.
  virtual int GetRemovalMask() const OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.clear.passwords")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_CLEAR_API_H_
