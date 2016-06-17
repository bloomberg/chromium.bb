// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_MOJO_APP_H_
#define CONTENT_PUBLIC_TEST_TEST_MOJO_APP_H_

#include <string>

#include "base/macros.h"
#include "content/public/test/test_mojo_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"

namespace content {

extern const char kTestMojoAppUrl[];

// Simple Mojo app which provides a mojom::TestMojoService impl. The app
// terminates itself after its TestService fulfills a single DoSomething call.
class TestMojoApp : public shell::ShellClient,
                    public shell::InterfaceFactory<mojom::TestMojoService>,
                    public mojom::TestMojoService {
 public:
  TestMojoApp();
  ~TestMojoApp() override;

 private:
  // shell::ShellClient:
  bool AcceptConnection(shell::Connection* connection) override;

  // shell::InterfaceFactory<mojom::TestMojoService>:
  void Create(shell::Connection* connection,
              mojo::InterfaceRequest<mojom::TestMojoService> request) override;

  // TestMojoService:
  void DoSomething(const DoSomethingCallback& callback) override;
  void DoTerminateProcess(const DoTerminateProcessCallback& callback) override;
  void CreateFolder(const CreateFolderCallback& callback) override;
  void GetRequestorName(const GetRequestorNameCallback& callback) override;

  mojo::Binding<mojom::TestMojoService> service_binding_;

  // The name of the app connecting to us.
  std::string requestor_name_;

  DISALLOW_COPY_AND_ASSIGN(TestMojoApp);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_MOJO_APP_H_
