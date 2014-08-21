// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/application_runner.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "mojo/services/public/interfaces/network/network_service.mojom.h"
#include "mojo/services/public/interfaces/network/url_loader.mojom.h"

namespace mojo {
namespace examples {
namespace {

class ResponsePrinter {
 public:
  void Run(URLResponsePtr response) const {
    if (response->error) {
      printf("Got error: %d (%s)\n",
          response->error->code, response->error->description.get().c_str());
    } else {
      PrintResponse(response);
      PrintResponseBody(response->body.Pass());
    }

    RunLoop::current()->Quit();  // All done!
  }

  void PrintResponse(const URLResponsePtr& response) const {
    printf(">>> Headers <<< \n");
    printf("  %s\n", response->status_line.get().c_str());
    if (response->headers) {
      for (size_t i = 0; i < response->headers.size(); ++i)
        printf("  %s\n", response->headers[i].get().c_str());
    }
  }

  void PrintResponseBody(ScopedDataPipeConsumerHandle body) const {
    // Read response body in blocking fashion.
    printf(">>> Body <<<\n");

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
    }

    printf("\n>>> EOF <<<\n");
  }
};

}  // namespace

class WGetApp : public ApplicationDelegate {
 public:
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE {
    app->ConnectToService("mojo:mojo_network_service", &network_service_);
    Start();
  }

 private:
  void Start() {
    std::string url = PromptForURL();
    printf("Loading: %s\n", url.c_str());

    network_service_->CreateURLLoader(Get(&url_loader_));

    URLRequestPtr request(URLRequest::New());
    request->url = url;
    request->method = "GET";
    request->auto_follow_redirects = true;

    url_loader_->Start(request.Pass(), ResponsePrinter());
  }

  std::string PromptForURL() {
    printf("Enter URL> ");
    char buf[1024];
    if (scanf("%1023s", buf) != 1)
      buf[0] = '\0';
    return buf;
  }

  NetworkServicePtr network_service_;
  URLLoaderPtr url_loader_;
};

}  // namespace examples
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new mojo::examples::WGetApp);
  return runner.Run(shell_handle);
}
