// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_TEST_SERVICE_TEST_SERVICE_APPLICATION_H_
#define MOJO_SERVICES_TEST_SERVICE_TEST_SERVICE_APPLICATION_H_

#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
class ApplicationConnection;

namespace test {
class TestService;
class TestTimeService;

class TestServiceApplication : public ApplicationDelegate,
                               public InterfaceFactory<TestService>,
                               public InterfaceFactory<TestTimeService> {
 public:
  TestServiceApplication();
  virtual ~TestServiceApplication();

  // ApplicationDelegate implementation.
  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE;

  // InterfaceFactory<TestService> implementation.
  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<TestService> request) MOJO_OVERRIDE;

  // InterfaceFactory<TestTimeService> implementation.
  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<TestTimeService> request) MOJO_OVERRIDE;

  void AddRef();
  void ReleaseRef();

 private:
  int ref_count_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(TestServiceApplication);
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_SERVICES_TEST_SERVICE_TEST_SERVICE_APPLICATION_H_
