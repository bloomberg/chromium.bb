// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_TAB_SOCKET_H_
#define HEADLESS_PUBLIC_HEADLESS_TAB_SOCKET_H_

#include <string>

#include "base/macros.h"
#include "headless/public/headless_export.h"

namespace headless {

// A bidirectional communications channel between C++ and JS.
class HEADLESS_EXPORT HeadlessTabSocket {
 public:
  class HEADLESS_EXPORT Listener {
   public:
    Listener() {}
    virtual ~Listener() {}

    // The |message| may be potentially sent by untrusted web content so it
    // should be validated carefully.
    virtual void OnMessageFromTab(const std::string& message) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Listener);
  };

  virtual void SendMessageToTab(const std::string& message) = 0;

  virtual void SetListener(Listener* listener) = 0;

 protected:
  HeadlessTabSocket() {}
  virtual ~HeadlessTabSocket() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessTabSocket);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_TAB_SOCKET_H_
