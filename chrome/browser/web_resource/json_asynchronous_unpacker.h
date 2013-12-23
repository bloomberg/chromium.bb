// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_RESOURCE_JSON_ASYNCHRONOUS_UNPACKER_H_
#define CHROME_BROWSER_WEB_RESOURCE_JSON_ASYNCHRONOUS_UNPACKER_H_

#include <string>

#include "base/values.h"

// A delegate interface for users of JSONAsynchronousUnpacker.
class JSONAsynchronousUnpackerDelegate {
 public:
  virtual ~JSONAsynchronousUnpackerDelegate() {}

  // This will be called when the response is parsed properly. |parsed_json|
  // contains the decoded information.
  virtual void OnUnpackFinished(const base::DictionaryValue& parsed_json) = 0;

  // This will be called if there is an error while parsing the data.
  virtual void OnUnpackError(const std::string& error_message) = 0;
};

// This class coordinates a web resource unpack and parse task which is run
// asynchronously.  Results are sent back to the delegate via one of its
// OnUnpack<xxx> methods.
class JSONAsynchronousUnpacker {
 public:
  // Creates a WebResourceServiceUnpacker appropriate for the platform. The
  // delegate must be kept around until one of the delegate methods is called.
  // In case the delegate is no longer valid, ClearDelegate() must be called.
  static JSONAsynchronousUnpacker*
      Create(JSONAsynchronousUnpackerDelegate* delegate);

  virtual ~JSONAsynchronousUnpacker() {}

  // Start the decoding. The concrete implementation should delete itself after
  // calling the delegate method.
  virtual void Start(const std::string& json_data) = 0;

  // If the delegate is going away it needs to inform the unpacker about it.
  void ClearDelegate() {
    delegate_ = NULL;
  };

 protected:
  explicit JSONAsynchronousUnpacker(JSONAsynchronousUnpackerDelegate* delegate)
      : delegate_(delegate) {
  }

  JSONAsynchronousUnpackerDelegate* delegate_;
};

#endif  // CHROME_BROWSER_WEB_RESOURCE_JSON_ASYNCHRONOUS_UNPACKER_H_
