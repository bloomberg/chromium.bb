// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/nacl_security_tests/nacl_security_tests_win.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>

// TODO(jvoung): factor out the enum SboxTestResult from
// "sandbox/tests/common/controller.h" to make it OS independent

#include "sandbox/tests/common/controller.h"
#include "sandbox/tests/validation_tests/commands.h"

BOOL APIENTRY DllMain(HMODULE module, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  return TRUE;
}

#define RETURN_IF_NOT_DENIED(x) \
  if (sandbox::SBOX_TEST_DENIED != x) { \
    return false; \
  }

////////////////////////////////////////////////////////////
// Additional sandbox access tests
// (not in "sandbox/tests/validation_tests/commands.h")
////////////////////////////////////////////////////////////

namespace sandbox {

SboxTestResult TestCreateProcess(const wchar_t *path_str, wchar_t *cmd_str) {
  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  ZeroMemory(&si, sizeof(si));
  ZeroMemory(&pi, sizeof(pi));
  if (!::CreateProcess(path_str, cmd_str, NULL, NULL, FALSE, 0,
                       NULL, NULL, &si, &pi)) {
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      fprintf(stderr, "Create process denied\n");
      return SBOX_TEST_DENIED;
    } else {
      fprintf(stderr, "Created process denied for misc reason: %ld\n",
              ::GetLastError());
      return SBOX_TEST_DENIED;
    }
  } else {
    fprintf(stderr, "Created process\n");
    return SBOX_TEST_SUCCEEDED;
  }
}

SboxTestResult TestConnect(const char* url) {
  WSADATA wsaData;
  int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (NO_ERROR != iResult) {
    fprintf(stderr, "Error at WSAStartup()\n");
  }

  struct addrinfo hints, *servinfo, *p;
  DWORD dwRet;
  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  dwRet = getaddrinfo(url, "80", &hints, &servinfo);
  if (0 != dwRet) {
    fprintf(stderr, "getaddrinfo failed with %d\n", dwRet);
    WSACleanup();
    return SBOX_TEST_DENIED;
  }

  p = servinfo;
  // Just try the first entry.
  SOCKET sock;
  sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
  if (INVALID_SOCKET == sock) {
    fprintf(stderr, "Error at socket(): %ld\n", WSAGetLastError());
    freeaddrinfo(servinfo);
    WSACleanup();
    return SBOX_TEST_DENIED;
  }

  if (SOCKET_ERROR == connect(sock, p->ai_addr,
                              static_cast<int>(p->ai_addrlen))) {
    fprintf(stderr, "Failed to connect\n");
    freeaddrinfo(servinfo);
    closesocket(sock);
    WSACleanup();
    return SBOX_TEST_DENIED;
  }

  fprintf(stderr, "OOPS: Connected to server.\n");
  freeaddrinfo(servinfo);
  closesocket(sock);
  WSACleanup();
  return SBOX_TEST_SUCCEEDED;
}

}  // namespace sandbox

////////////////////////////////////////////////////////////


// Runs the security tests of sandbox for the nacl loader process.
// If a test fails, the return value is FALSE and test_count contains the
// number of tests executed, including the failing test.
extern "C" bool __declspec(dllexport) RunNaClLoaderTests(void) {
  // Filesystem and Registry tests borrowed from renderer security_tests.dll
  RETURN_IF_NOT_DENIED(sandbox::TestOpenReadFile(L"%SystemDrive%"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenReadFile(L"%SystemRoot%"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenReadFile(L"%ProgramFiles%"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenReadFile(L"%SystemRoot%\\System32"));
  RETURN_IF_NOT_DENIED(
    sandbox::TestOpenReadFile(L"%SystemRoot%\\explorer.exe"));
  RETURN_IF_NOT_DENIED(
    sandbox::TestOpenReadFile(L"%SystemRoot%\\Cursors\\arrow_i.cur"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenWriteFile(L"%SystemRoot%"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenWriteFile(L"%ProgramFiles%"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenWriteFile(L"%SystemRoot%\\System32"));
  RETURN_IF_NOT_DENIED(
    sandbox::TestOpenWriteFile(L"%SystemRoot%\\explorer.exe"));
  RETURN_IF_NOT_DENIED(
    sandbox::TestOpenWriteFile(L"%SystemRoot%\\Cursors\\arrow_i.cur"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenReadFile(L"%AllUsersProfile%"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenReadFile(L"%Temp%"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenReadFile(L"%AppData%"));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenKey(HKEY_LOCAL_MACHINE, L""));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenKey(HKEY_CURRENT_USER, L""));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenKey(HKEY_USERS, L""));
  RETURN_IF_NOT_DENIED(sandbox::TestOpenKey(HKEY_LOCAL_MACHINE,
      L"Software\\Microsoft\\Windows NT\\CurrentVersion\\WinLogon"));

  RETURN_IF_NOT_DENIED(sandbox::TestConnect("www.archive.org"));
  RETURN_IF_NOT_DENIED(sandbox::TestConnect("www.google.com"));

  RETURN_IF_NOT_DENIED(sandbox::TestCreateProcess(L"%SystemRoot%\\explorer.exe",
                                         L"%SystemRoot%\\explorer.exe"));

  return true;
}
