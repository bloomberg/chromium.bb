// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/nacl_security_tests/commands_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <string>

// Sandbox access tests (mimic'ing "sandbox/tests/validation_tests/commands.h")

namespace sandbox {

// Permissions for the user to read & write and others to read.
const mode_t kCreatePermissions = 0644;

SboxTestResult TestOpenReadFile(const char *path) {
  int fd = open(path, O_RDONLY | O_CREAT, kCreatePermissions);
  if (-1 == fd) {
    return SBOX_TEST_DENIED;
  } else {
    close(fd);
    return SBOX_TEST_SUCCEEDED;
  }
}

SboxTestResult TestOpenWriteFile(const char *path) {
  int fd = open(path, O_WRONLY | O_CREAT, kCreatePermissions);
  if (-1 == fd) {
    return SBOX_TEST_DENIED;
  } else {
    close(fd);
    return SBOX_TEST_SUCCEEDED;
  }
}

SboxTestResult TestCreateProcess(const char *path) {
  pid_t pid;
  int exec_res;

  pid = fork();
  if (0 == pid) {
    exec_res = execl(path, path, NULL);
    if (exec_res) {
      return SBOX_TEST_DENIED;
    } else {
      return SBOX_TEST_SUCCEEDED;
    }
    return SBOX_TEST_SUCCEEDED;
  } else if (0 < pid) {
    pid_t w_pid;
    do {
      w_pid = waitpid(pid, NULL, WNOHANG);
    } while (w_pid != -1 && errno != EINTR);
    return SBOX_TEST_SUCCEEDED;
  } else {
    return SBOX_TEST_DENIED;
  }
}

SboxTestResult TestConnect(const char *url) {
  int conn_sock;
  struct addrinfo hints, *servinfo, *p;
  int rv;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  rv = getaddrinfo(url, "http", &hints, &servinfo);
  if (0 != rv) {
    return SBOX_TEST_DENIED;
  }

  p = servinfo;
  // Just try the first entry.
  conn_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
  if (-1 == conn_sock) {
    perror("socket");
    freeaddrinfo(servinfo);
    return SBOX_TEST_DENIED;
  }

  if (-1 == connect(conn_sock, p->ai_addr, p->ai_addrlen)) {
    close(conn_sock);
    freeaddrinfo(servinfo);
    return SBOX_TEST_DENIED;
  }

  shutdown(conn_sock, SHUT_RDWR);
  close(conn_sock);
  freeaddrinfo(servinfo);
  return SBOX_TEST_SUCCEEDED;
}

// TODO(jvoung): test more: e.g., bind and accept.
// chmod, unlink, symlink, ... if guaranteed a test file that would normally
// allow us to do such things (i.e., we want the test operations to be
// context-independent, yet leave no traces).

SboxTestResult TestDummyFails() {
  return SBOX_TEST_SUCCEEDED;
}

}  // namespace sandbox
