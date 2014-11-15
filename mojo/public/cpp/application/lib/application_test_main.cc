// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/application_test_base.h"
#include "mojo/public/cpp/environment/logging.h"
#include "mojo/public/cpp/system/message_pipe.h"

MojoResult MojoMain(MojoHandle shell_handle) {
  // An Environment instance is needed to construct run loops.
  mojo::Environment environment;

  {
    // This RunLoop is used for init, and then destroyed before running tests.
    mojo::Environment::InstantiateDefaultRunLoop();

    // Construct an ApplicationImpl just for the GTEST commandline arguments.
    // GTEST command line arguments are supported amid application arguments:
    // $ mojo_shell 'mojo:example_apptests arg1 --gtest_filter=foo arg2'
    mojo::ApplicationDelegate dummy_application_delegate;
    mojo::ApplicationImpl app(&dummy_application_delegate, shell_handle);
    MOJO_CHECK(app.WaitForInitialize());

    // InitGoogleTest expects (argc + 1) elements, including a terminating NULL.
    // It also removes GTEST arguments from |argv| and updates the |argc| count.
    // TODO(msw): Provide tests access to these actual command line arguments.
    const std::vector<std::string>& args = app.args();
    MOJO_CHECK(args.size() <
               static_cast<size_t>(std::numeric_limits<int>::max()));
    int argc = static_cast<int>(args.size());
    std::vector<const char*> argv(argc + 1);
    for (int i = 0; i < argc; ++i)
      argv[i] = args[i].c_str();
    argv[argc] = nullptr;

    testing::InitGoogleTest(&argc, const_cast<char**>(&(argv[0])));
    mojo::test::SetShellHandle(app.UnbindShell());
    mojo::Environment::DestroyDefaultRunLoop();
  }

  int result = RUN_ALL_TESTS();

  shell_handle = mojo::test::PassShellHandle().release().value();
  MojoResult close_result = MojoClose(shell_handle);
  MOJO_CHECK(close_result == MOJO_RESULT_OK);

  return (result == 0) ? MOJO_RESULT_OK : MOJO_RESULT_UNKNOWN;
}
