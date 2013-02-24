// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_LAYOUT_TEST_HTTP_SERVER_H_
#define CONTENT_TEST_LAYOUT_TEST_HTTP_SERVER_H_

#include "base/compiler_specific.h"
#include "base/files/file_path.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace content {

// This object bounds the lifetime of an external HTTP server
// used for layout tests.
//
// NOTE: If you're not running a layout test, you probably want
// a more lightweight net/test/test_server HTTP server.
class LayoutTestHttpServer {
 public:
  LayoutTestHttpServer(const base::FilePath& root_directory, int port);
  ~LayoutTestHttpServer();

  // Starts the server. Returns true on success.
  bool Start() WARN_UNUSED_RESULT;

  // Stops the server. Returns true on success.
  //
  // NOTE: It is recommended to explicitly call Stop and check its return value.
  // If Stop fails, the server is most likely still running and future attempts
  // to bind to the same port will fail, possibly resulting in further test
  // failures.
  bool Stop() WARN_UNUSED_RESULT;

  int port() const { return port_; }

 private:
  base::FilePath root_directory_;  // Root directory of the server.

  int port_;  // Port on which the server should listen.

  bool running_;  // True if the server is currently running.

#if defined(OS_WIN)
  // JobObject used to clean up orphaned child processes.
  base::win::ScopedHandle job_handle_;
#endif

  DISALLOW_COPY_AND_ASSIGN(LayoutTestHttpServer);
};

}  // namespace content

#endif  // CONTENT_TEST_LAYOUT_TEST_HTTP_SERVER_H_
