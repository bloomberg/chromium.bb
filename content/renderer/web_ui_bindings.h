// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_UI_BINDINGS_H_
#define CONTENT_RENDERER_WEB_UI_BINDINGS_H_
#pragma once

#include "content/common/content_export.h"
#include "ipc/ipc_message.h"
#include "webkit/glue/cpp_bound_class.h"

// A DOMBoundBrowserObject is a backing for some object bound to the window
// in JS that knows how to dispatch messages to an associated c++ object living
// in the browser process.
class DOMBoundBrowserObject : public webkit_glue::CppBoundClass {
 public:
  CONTENT_EXPORT DOMBoundBrowserObject();
  CONTENT_EXPORT virtual ~DOMBoundBrowserObject();

  // Sets a property with the given name and value.
  void SetProperty(const std::string& name, const std::string& value);

 private:
  // The list of properties that have been set.  We keep track of this so we
  // can free them on destruction.
  typedef std::vector<webkit_glue::CppVariant*> PropertyList;
  PropertyList properties_;

  DISALLOW_COPY_AND_ASSIGN(DOMBoundBrowserObject);
};

// WebUIBindings is the class backing the "chrome" object accessible
// from Javascript from privileged pages.
//
// We expose one function, for sending a message to the browser:
//   send(String name, Object argument);
// It's plumbed through to the OnWebUIMessage callback on RenderViewHost
// delegate.
class WebUIBindings : public DOMBoundBrowserObject {
 public:
  WebUIBindings(IPC::Message::Sender* sender,
                int routing_id);
  virtual ~WebUIBindings();

 private:
  // The send() function provided to Javascript.
  void Send(const webkit_glue::CppArgumentList& args,
            webkit_glue::CppVariant* result);

  IPC::Message::Sender* sender_;
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(WebUIBindings);
};

#endif  // CONTENT_RENDERER_WEB_UI_BINDINGS_H_
