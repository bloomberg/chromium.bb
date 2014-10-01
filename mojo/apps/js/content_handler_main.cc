// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/i18n/icu_util.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/apps/js/application_delegate_impl.h"
#include "mojo/apps/js/content_handler_impl.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/public/interfaces/content_handler/content_handler.mojom.h"

namespace mojo {
namespace apps {

class ContentHandlerApplicationDelegateImpl : public ApplicationDelegateImpl {
 public:
  ContentHandlerApplicationDelegateImpl() : content_handler_factory_(this) {
  }

 private:
  virtual bool ConfigureIncomingConnection(
      ApplicationConnection* connection) override {
    connection->AddService(&content_handler_factory_);
    return true;
  }

  InterfaceFactoryImplWithContext<ContentHandlerImpl, ApplicationDelegateImpl>
      content_handler_factory_;
};

}  // namespace apps
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  base::i18n::InitializeICU();
  mojo::ApplicationRunnerChromium runner(
      new mojo::apps::ContentHandlerApplicationDelegateImpl());
  return runner.Run(shell_handle);
}
