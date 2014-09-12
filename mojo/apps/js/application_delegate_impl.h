// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPS_JS_CONTENT_HANDLER_H_
#define MOJO_APPS_JS_CONTENT_HANDLER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/public/interfaces/content_handler/content_handler.mojom.h"

namespace mojo {

class ApplcationImpl;

namespace apps {

class ApplicationDelegateImpl;
class JSApp;

// Starts a new JSApp for each OnConnect call().

class ContentHandlerImpl : public InterfaceImpl<ContentHandler> {
 public:
  ContentHandlerImpl(ApplicationDelegateImpl* content_handler);
  virtual ~ContentHandlerImpl();

 private:
  virtual void OnConnect(const mojo::String& url,
                         URLResponsePtr content,
                         InterfaceRequest<ServiceProvider> service_provider)
      MOJO_OVERRIDE;

  ApplicationDelegateImpl* content_handler_;
};

// Manages the JSApps started by this content handler. This class owns the one
// reference to the Mojo shell. JSApps post a task to the content handler's
// thread to connect to a service or to quit.l

class ApplicationDelegateImpl : public ApplicationDelegate {
 public:
  ApplicationDelegateImpl();
  virtual ~ApplicationDelegateImpl();

  void StartJSApp(const std::string& url, URLResponsePtr content);
  void QuitJSApp(JSApp* app);

  void ConnectToService(ScopedMessagePipeHandle pipe_handle,
                        const std::string& application_url,
                        const std::string& interface_name);

 private:
  typedef ScopedVector<JSApp> AppVector;

  // ApplicationDelegate methods
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE;
  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE;

  ApplicationImpl* application_impl_;
  InterfaceFactoryImplWithContext<ContentHandlerImpl, ApplicationDelegateImpl>
      content_handler_factory_;
  AppVector app_vector_;
};

}  // namespace apps
}  // namespace mojo

#endif  // MOJO_APPS_JS_CONTENT_HANDLER_H_
