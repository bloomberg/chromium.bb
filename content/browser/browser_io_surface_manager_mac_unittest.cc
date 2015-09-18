// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_io_surface_manager_mac.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class BrowserIOSurfaceManagerTest : public testing::Test {
 public:
  void EnsureRunning() { io_surface_manager_.EnsureRunning(); }

  IOSurfaceManagerToken GenerateGpuProcessToken() {
    return io_surface_manager_.GenerateGpuProcessToken();
  }

  void InvalidateGpuProcessToken() {
    io_surface_manager_.InvalidateGpuProcessToken();
  }

  IOSurfaceManagerToken GenerateChildProcessToken(int child_process_id) {
    return io_surface_manager_.GenerateChildProcessToken(child_process_id);
  }

  void InvalidateChildProcessToken(const IOSurfaceManagerToken& token) {
    io_surface_manager_.InvalidateChildProcessToken(token);
  }

  bool HandleRegisterIOSurfaceRequest(
      const IOSurfaceManagerHostMsg_RegisterIOSurface& request,
      IOSurfaceManagerMsg_RegisterIOSurfaceReply* reply) {
    io_surface_manager_.HandleRegisterIOSurfaceRequest(request, reply);
    return reply->result;
  }

  bool HandleUnregisterIOSurfaceRequest(
      const IOSurfaceManagerHostMsg_UnregisterIOSurface& request) {
    return io_surface_manager_.HandleUnregisterIOSurfaceRequest(request);
  }

  bool HandleAcquireIOSurfaceRequest(
      const IOSurfaceManagerHostMsg_AcquireIOSurface& request,
      IOSurfaceManagerMsg_AcquireIOSurfaceReply* reply) {
    io_surface_manager_.HandleAcquireIOSurfaceRequest(request, reply);
    return reply->result;
  }

  // Helper function used to register an IOSurface for testing.
  void RegisterIOSurface(const IOSurfaceManagerToken& gpu_process_token,
                         int io_surface_id,
                         int client_id,
                         mach_port_t io_surface_port) {
    IOSurfaceManagerHostMsg_RegisterIOSurface request = {{0}};
    IOSurfaceManagerMsg_RegisterIOSurfaceReply reply = {{0}};
    request.io_surface_id = io_surface_id;
    request.client_id = client_id;
    request.io_surface_port.name = io_surface_port;
    memcpy(request.token_name, gpu_process_token.name,
           sizeof(gpu_process_token.name));
    ASSERT_TRUE(HandleRegisterIOSurfaceRequest(request, &reply));
    ASSERT_TRUE(reply.result);
  }

 private:
  BrowserIOSurfaceManager io_surface_manager_;
};

TEST_F(BrowserIOSurfaceManagerTest, EnsureRunning) {
  EnsureRunning();
  base::mac::ScopedMachSendRight service_port =
      BrowserIOSurfaceManager::LookupServicePort(getpid());
  EXPECT_TRUE(service_port.is_valid());
}

TEST_F(BrowserIOSurfaceManagerTest, RegisterIOSurfaceSucceeds) {
  const int kIOSurfaceId = 1;
  const int kClientId = 1;

  IOSurfaceManagerToken gpu_process_token = GenerateGpuProcessToken();

  IOSurfaceManagerHostMsg_RegisterIOSurface request = {{0}};
  IOSurfaceManagerMsg_RegisterIOSurfaceReply reply = {{0}};

  request.io_surface_id = kIOSurfaceId;
  request.client_id = kClientId;
  memcpy(request.token_name, gpu_process_token.name,
         sizeof(gpu_process_token.name));
  EXPECT_TRUE(HandleRegisterIOSurfaceRequest(request, &reply));
  EXPECT_TRUE(reply.result);
}

TEST_F(BrowserIOSurfaceManagerTest, RegisterIOSurfaceFailsWithInvalidToken) {
  const int kIOSurfaceId = 1;
  const int kClientId = 1;

  IOSurfaceManagerHostMsg_RegisterIOSurface request = {{0}};
  IOSurfaceManagerMsg_RegisterIOSurfaceReply reply = {{0}};
  request.io_surface_id = kIOSurfaceId;
  request.client_id = kClientId;
  // Fails as request doesn't contain a valid token.
  EXPECT_FALSE(HandleRegisterIOSurfaceRequest(request, &reply));
}

TEST_F(BrowserIOSurfaceManagerTest,
       RegisterIOSurfaceFailsWithChildProcessToken) {
  const int kIOSurfaceId = 1;
  const int kClientId = 1;

  IOSurfaceManagerToken child_process_token =
      GenerateChildProcessToken(kClientId);

  IOSurfaceManagerHostMsg_RegisterIOSurface request = {{0}};
  IOSurfaceManagerMsg_RegisterIOSurfaceReply reply = {{0}};
  request.io_surface_id = kIOSurfaceId;
  request.client_id = kClientId;
  memcpy(request.token_name, child_process_token.name,
         sizeof(child_process_token.name));
  // Fails as child process is not allowed to register IOSurfaces.
  EXPECT_FALSE(HandleRegisterIOSurfaceRequest(request, &reply));
}

TEST_F(BrowserIOSurfaceManagerTest, UnregisterIOSurfaceSucceeds) {
  const int kIOSurfaceId = 1;
  const int kClientId = 1;

  IOSurfaceManagerToken gpu_process_token = GenerateGpuProcessToken();

  RegisterIOSurface(gpu_process_token, kIOSurfaceId, kClientId, 0);

  IOSurfaceManagerHostMsg_UnregisterIOSurface request = {{0}};
  request.io_surface_id = kIOSurfaceId;
  request.client_id = kClientId;
  memcpy(request.token_name, gpu_process_token.name,
         sizeof(gpu_process_token.name));
  EXPECT_TRUE(HandleUnregisterIOSurfaceRequest(request));
}

TEST_F(BrowserIOSurfaceManagerTest, UnregisterIOSurfaceFailsWithInvalidToken) {
  const int kIOSurfaceId = 1;
  const int kClientId = 1;

  IOSurfaceManagerToken gpu_process_token = GenerateGpuProcessToken();

  RegisterIOSurface(gpu_process_token, kIOSurfaceId, kClientId, 0);

  IOSurfaceManagerHostMsg_UnregisterIOSurface request = {{0}};
  request.io_surface_id = kIOSurfaceId;
  request.client_id = kClientId;
  // Fails as request doesn't contain valid GPU token.
  EXPECT_FALSE(HandleUnregisterIOSurfaceRequest(request));
}

TEST_F(BrowserIOSurfaceManagerTest,
       UnregisterIOSurfaceFailsWithChildProcessToken) {
  const int kIOSurfaceId = 1;
  const int kClientId = 1;

  IOSurfaceManagerToken gpu_process_token = GenerateGpuProcessToken();
  IOSurfaceManagerToken child_process_token =
      GenerateChildProcessToken(kClientId);

  RegisterIOSurface(gpu_process_token, kIOSurfaceId, kClientId, 0);

  IOSurfaceManagerHostMsg_UnregisterIOSurface request = {{0}};
  request.io_surface_id = kIOSurfaceId;
  request.client_id = kClientId;
  memcpy(request.token_name, child_process_token.name,
         sizeof(child_process_token.name));
  // Fails as child process is not allowed to unregister IOSurfaces.
  EXPECT_FALSE(HandleUnregisterIOSurfaceRequest(request));
}

TEST_F(BrowserIOSurfaceManagerTest, AcquireIOSurfaceSucceeds) {
  const int kIOSurfaceId = 10;
  const int kClientId = 1;
  const mach_port_t kIOSurfacePortId = 100u;

  IOSurfaceManagerToken gpu_process_token = GenerateGpuProcessToken();
  IOSurfaceManagerToken child_process_token =
      GenerateChildProcessToken(kClientId);

  RegisterIOSurface(gpu_process_token, kIOSurfaceId, kClientId,
                    kIOSurfacePortId);

  IOSurfaceManagerHostMsg_AcquireIOSurface request = {{0}};
  IOSurfaceManagerMsg_AcquireIOSurfaceReply reply = {{0}};
  request.io_surface_id = kIOSurfaceId;
  memcpy(request.token_name, child_process_token.name,
         sizeof(child_process_token.name));
  EXPECT_TRUE(HandleAcquireIOSurfaceRequest(request, &reply));
  EXPECT_EQ(kIOSurfacePortId, reply.io_surface_port.name);
}

TEST_F(BrowserIOSurfaceManagerTest, AcquireIOSurfaceFailsWithInvalidToken) {
  const int kIOSurfaceId = 10;
  const int kClientId = 1;
  const mach_port_t kIOSurfacePortId = 100u;

  IOSurfaceManagerToken gpu_process_token = GenerateGpuProcessToken();

  RegisterIOSurface(gpu_process_token, kIOSurfaceId, kClientId,
                    kIOSurfacePortId);

  IOSurfaceManagerHostMsg_AcquireIOSurface request = {{0}};
  IOSurfaceManagerMsg_AcquireIOSurfaceReply reply = {{0}};
  request.io_surface_id = kIOSurfaceId;
  // Fails as request doesn't contain valid token.
  EXPECT_FALSE(HandleAcquireIOSurfaceRequest(request, &reply));
}

TEST_F(BrowserIOSurfaceManagerTest,
       AcquireIOSurfaceFailsWithWrongChildProcessToken) {
  const int kIOSurfaceId = 10;
  const int kClientId1 = 1;
  const int kClientId2 = 2;
  const mach_port_t kIOSurfacePortId = 100u;

  IOSurfaceManagerToken gpu_process_token = GenerateGpuProcessToken();
  IOSurfaceManagerToken child_process_token2 =
      GenerateChildProcessToken(kClientId2);

  RegisterIOSurface(gpu_process_token, kIOSurfaceId, kClientId1,
                    kIOSurfacePortId);

  IOSurfaceManagerHostMsg_AcquireIOSurface request = {{0}};
  IOSurfaceManagerMsg_AcquireIOSurfaceReply reply = {{0}};
  request.io_surface_id = kIOSurfaceId;
  memcpy(request.token_name, child_process_token2.name,
         sizeof(child_process_token2.name));
  EXPECT_TRUE(HandleAcquireIOSurfaceRequest(request, &reply));
  // Fails as child process 2 is not allowed to acquire this IOSurface.
  EXPECT_EQ(static_cast<mach_port_t>(MACH_PORT_NULL),
            reply.io_surface_port.name);
}

TEST_F(BrowserIOSurfaceManagerTest,
       AcquireIOSurfaceFailsAfterInvalidatingChildProcessToken) {
  const int kIOSurfaceId = 10;
  const int kClientId = 1;
  const mach_port_t kIOSurfacePortId = 100u;

  IOSurfaceManagerToken gpu_process_token = GenerateGpuProcessToken();
  IOSurfaceManagerToken child_process_token =
      GenerateChildProcessToken(kClientId);

  RegisterIOSurface(gpu_process_token, kIOSurfaceId, kClientId,
                    kIOSurfacePortId);

  InvalidateChildProcessToken(child_process_token);

  IOSurfaceManagerHostMsg_AcquireIOSurface request = {{0}};
  IOSurfaceManagerMsg_AcquireIOSurfaceReply reply = {{0}};
  request.io_surface_id = kIOSurfaceId;
  memcpy(request.token_name, child_process_token.name,
         sizeof(child_process_token.name));
  // Fails as token is no longer valid.
  EXPECT_FALSE(HandleAcquireIOSurfaceRequest(request, &reply));
}

TEST_F(BrowserIOSurfaceManagerTest,
       AcquireIOSurfaceAfterInvalidatingGpuProcessToken) {
  const int kIOSurfaceId = 10;
  const int kClientId = 1;
  const mach_port_t kIOSurfacePortId = 100u;

  IOSurfaceManagerToken gpu_process_token = GenerateGpuProcessToken();
  IOSurfaceManagerToken child_process_token =
      GenerateChildProcessToken(kClientId);

  RegisterIOSurface(gpu_process_token, kIOSurfaceId, kClientId,
                    kIOSurfacePortId);

  InvalidateGpuProcessToken();

  IOSurfaceManagerHostMsg_AcquireIOSurface request = {{0}};
  IOSurfaceManagerMsg_AcquireIOSurfaceReply reply = {{0}};
  request.io_surface_id = kIOSurfaceId;
  memcpy(request.token_name, child_process_token.name,
         sizeof(child_process_token.name));
  EXPECT_TRUE(HandleAcquireIOSurfaceRequest(request, &reply));
  // Should return invalid port.
  EXPECT_EQ(static_cast<mach_port_t>(MACH_PORT_NULL),
            reply.io_surface_port.name);
}

}  // namespace content
