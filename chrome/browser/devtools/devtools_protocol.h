// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/values.h"

// Utility class for processing DevTools remote debugging messages.
// This is a stripped down clone of content::DevTools which is not accessible
// from chrome component (see content/browser/devtools/devtools_protocol.*).
class DevToolsProtocol {
 public:
  class Message {
   public:
    virtual ~Message();

    std::string method() { return method_; }
    base::DictionaryValue* params() { return params_.get(); }

   protected:
    // Takes ownership of |params|.
    Message(const std::string& method, base::DictionaryValue* params);

    std::string method_;
    scoped_ptr<base::DictionaryValue> params_;

   private:
    DISALLOW_COPY_AND_ASSIGN(Message);
  };

  class Command : public Message {
   public:
    // Takes ownership of |params|.
    Command(int id, const std::string& method, base::DictionaryValue* params);
    virtual  ~Command();

    int id() { return id_; }
    std::string Serialize();

   private:
    int id_;

    DISALLOW_COPY_AND_ASSIGN(Command);
  };

  class Notification : public Message {
   public:
    virtual ~Notification();

   private:
    friend class DevToolsProtocol;

    // Takes ownership of |params|.
    Notification(const std::string& method,
                 base::DictionaryValue* params);

    DISALLOW_COPY_AND_ASSIGN(Notification);
  };

  static Notification* ParseNotification(const std::string& json);

 private:

  DevToolsProtocol() {}
  ~DevToolsProtocol() {}
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_H_
