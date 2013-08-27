// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_RESOURCE_CONVERTER_H
#define CONTENT_RENDERER_PEPPER_RESOURCE_CONVERTER_H

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"
#include "v8/include/v8.h"

namespace IPC {
class Message;
}

namespace ppapi {
class ScopedPPVar;
}

namespace content {

class RendererPpapiHost;

// This class is responsible for converting V8 vars to Pepper resources.
class CONTENT_EXPORT ResourceConverter {
 public:
  virtual ~ResourceConverter();

  // Flush() must be called before any vars created by the ResourceConverter
  // are valid. It handles creating any resource hosts that need to be created.
  virtual void Flush(const base::Callback<void(bool)>& callback) = 0;
};

class ResourceConverterImpl : public ResourceConverter {
 public:
  ResourceConverterImpl(PP_Instance instance, RendererPpapiHost* host);
  virtual ~ResourceConverterImpl();
  virtual void Flush(const base::Callback<void(bool)>& callback) OVERRIDE;

  bool FromV8Value(v8::Handle<v8::Value> val,
                   v8::Handle<v8::Context> context,
                   PP_Var* result);

 private:
  // Creates a resource var with the given message to be sent to the plugin.
  PP_Var CreateResourceVar(const IPC::Message& create_message);
  // Creates a resource var with the given message to send to the plugin and a
  // message to create the browser host.
  PP_Var CreateResourceVarWithBrowserHost(
      const IPC::Message& create_message,
      const IPC::Message& browser_host_create_message);

  // The instance this ResourceConverter is associated with.
  PP_Instance instance_;
  // The RendererPpapiHost to use to create browser hosts.
  RendererPpapiHost* host_;

  // A list of the messages to create the browser hosts. This is a parallel
  // array to |browser_host_resource_vars|. It is kept as a parallel array so
  // that it can be conveniently passed to |CreateBrowserResourceHosts|.
  std::vector<IPC::Message> browser_host_create_messages_;
  // A list of the resource vars associated with browser hosts.
  std::vector<PP_Var> browser_host_resource_vars;

  DISALLOW_COPY_AND_ASSIGN(ResourceConverterImpl);
};

}  // namespace content
#endif  // CONTENT_RENDERER_PEPPER_RESOURCE_CONVERTER_H
