/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#ifndef AF_IPX
#include <winsock2.h>
#endif

#include "native_client/src/debug_server/debug_stub/debug_stub.h"
#include "native_client/src/debug_server/port/platform.h"


using port::IPlatform;

void NaClDebugStubPlatformInit() {
  WORD wVersionRequested;
  WSADATA wsaData;
  int err;

  // Make sure to request the use of sockets.
  // NOTE:  It is safe to call Startup multiple times
  wVersionRequested = MAKEWORD(2, 2);
  err = WSAStartup(wVersionRequested, &wsaData);
  if (err != 0) {
    // We could not find a matching DLL
    IPlatform::LogError("WSAStartup failed with error: %d\n", err);
    exit(-1);
  }

  if (HIBYTE(wsaData.wVersion) != 2) {
    // We couldn't get a matching version
    IPlatform::LogError("Could not find a usable version of Winsock.dll\n");
    WSACleanup();
    exit(-1);
  }
}

void NaClDebugStubPlatformFini() {
  WSACleanup();
}

