// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_TEST_EXAMPLE_CLIENT_APPLICATION_H_
#define MOJO_EXAMPLES_TEST_EXAMPLE_CLIENT_APPLICATION_H_

#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {

class ExampleClientApplication : public ApplicationDelegate {
 public:
  ExampleClientApplication();
  virtual ~ExampleClientApplication();

 private:
  // ApplicationDelegate implementation.
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ExampleClientApplication);
};

}  // namespace mojo

#endif  // MOJO_EXAMPLES_TEST_EXAMPLE_CLIENT_APPLICATION_H_
