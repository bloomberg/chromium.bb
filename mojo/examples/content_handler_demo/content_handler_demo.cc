// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/public/interfaces/content_handler/content_handler.mojom.h"

namespace mojo {
namespace examples {

class ContentHandlerApp;

class ContentHandlerImpl : public InterfaceImpl<ContentHandler> {
 public:
  explicit ContentHandlerImpl(ContentHandlerApp* content_handler_app)
      : content_handler_app_(content_handler_app) {
  }
  virtual ~ContentHandlerImpl() {}

 private:
  virtual void OnConnect(const mojo::String& url,
                         URLResponsePtr response,
                         InterfaceRequest<ServiceProvider> service_provider)
      MOJO_OVERRIDE;

  ContentHandlerApp* content_handler_app_;
};

class ContentHandlerApp : public ApplicationDelegate {
 public:
  ContentHandlerApp() : content_handler_factory_(this) {
  }

  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE {
  }

  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    connection->AddService(&content_handler_factory_);
    return true;
  }

  void PrintResponse(ScopedDataPipeConsumerHandle body) const {
    for (;;) {
      char buf[512];
      uint32_t num_bytes = sizeof(buf);
      MojoResult result = ReadDataRaw(body.get(), buf, &num_bytes,
                                      MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        Wait(body.get(),
             MOJO_HANDLE_SIGNAL_READABLE,
             MOJO_DEADLINE_INDEFINITE);
      } else if (result == MOJO_RESULT_OK) {
        if (fwrite(buf, num_bytes, 1, stdout) != 1) {
          printf("\nUnexpected error writing to file\n");
          break;
        }
      } else {
        break;
      }

      printf("\n>>> EOF <<<\n");
    }
  }

 private:
  InterfaceFactoryImplWithContext<ContentHandlerImpl,
                                  ContentHandlerApp> content_handler_factory_;
};

void ContentHandlerImpl::OnConnect(
    const mojo::String& url,
    URLResponsePtr response,
    InterfaceRequest<ServiceProvider> service_provider) {
  printf("ContentHandler::OnConnect - url:%s - body follows\n\n",
         url.To<std::string>().c_str());
  content_handler_app_->PrintResponse(response->body.Pass());
}

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::ContentHandlerApp();
}

}  // namespace mojo
