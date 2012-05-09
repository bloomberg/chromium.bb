// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTERNAL_HOST_BINDINGS_H_
#define CHROME_RENDERER_EXTERNAL_HOST_BINDINGS_H_
#pragma once

#include "ipc/ipc_message.h"
#include "webkit/glue/cpp_bound_class.h"

// ExternalHostBindings is the class backing the "externalHost" object
// accessible from Javascript
//
// We expose one function, for sending a message to the external host:
//  postMessage(String message[, String target]);
class ExternalHostBindings : public webkit_glue::CppBoundClass {
 public:
  ExternalHostBindings(IPC::Message::Sender* sender, int routing_id);
  virtual ~ExternalHostBindings();

  // Invokes the registered onmessage handler.
  // Returns true on successful invocation.
  bool ForwardMessageFromExternalHost(const std::string& message,
                                      const std::string& origin,
                                      const std::string& target);

  // Overridden to hold onto a pointer back to the web frame.
  void BindToJavascript(WebKit::WebFrame* frame, const std::string& classname);

 private:
  // Creates an uninitialized instance of a MessageEvent object.
  // This is equivalent to calling window.document.createEvent("MessageEvent")
  // in javascript.
  bool CreateMessageEvent(NPObject** message_event);

  // The postMessage() function provided to Javascript.
  void PostMessage(const webkit_glue::CppArgumentList& args,
                   webkit_glue::CppVariant* result);

  webkit_glue::CppVariant on_message_handler_;
  WebKit::WebFrame* frame_;
  IPC::Message::Sender* sender_;
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(ExternalHostBindings);
};

#endif  // CHROME_RENDERER_EXTERNAL_HOST_BINDINGS_H_
