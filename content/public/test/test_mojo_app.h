// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_MOJO_APP_H_
#define CONTENT_PUBLIC_TEST_TEST_MOJO_APP_H_

#include "base/macros.h"
#include "content/public/test/test_mojo_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "url/gurl.h"

namespace content {

extern const char kTestMojoAppUrl[];

// Simple Mojo app which provides a TestMojoService impl. The app terminates
// itself after its TestService fulfills a single DoSomething call.
class TestMojoApp : public mojo::ApplicationDelegate,
                    public mojo::InterfaceFactory<TestMojoService>,
                    public TestMojoService {
 public:
  TestMojoApp();
  ~TestMojoApp() override;

 private:
  // mojo::ApplicationDelegate:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override;
  bool AcceptConnection(
      mojo::ApplicationConnection* connection) override;

  // mojo::InterfaceFactory<TestMojoService>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<TestMojoService> request) override;

  // TestMojoService:
  void DoSomething(const DoSomethingCallback& callback) override;
  void GetRequestorURL(const GetRequestorURLCallback& callback) override;

  mojo::Binding<TestMojoService> service_binding_;

  // Not owned.
  mojo::Shell* shell_;

  // The URL of the app connecting to us.
  GURL requestor_url_;

  DISALLOW_COPY_AND_ASSIGN(TestMojoApp);
};

}  // namespace

#endif  // CONTENT_PUBLIC_TEST_TEST_MOJO_APP_H_
