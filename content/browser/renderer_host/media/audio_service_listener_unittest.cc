// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_service_listener.h"

#include "services/audio/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

using RunningServiceInfoPtr = service_manager::mojom::RunningServiceInfoPtr;

namespace content {

namespace {

RunningServiceInfoPtr MakeTestServiceInfo(
    const service_manager::Identity& identity,
    uint32_t pid) {
  RunningServiceInfoPtr info(service_manager::mojom::RunningServiceInfo::New());
  info->identity = identity;
  info->pid = pid;
  return info;
}

}  // namespace

TEST(AudioServiceListenerTest, OnInitWithoutAudioService_ProcessIdNull) {
  AudioServiceListener audio_service_listener(nullptr);
  service_manager::Identity id("id1");
  constexpr base::ProcessId pid(42);
  std::vector<RunningServiceInfoPtr> instances;
  instances.push_back(MakeTestServiceInfo(id, pid));
  audio_service_listener.OnInit(std::move(instances));
  EXPECT_EQ(base::kNullProcessId, audio_service_listener.GetProcessId());
}

TEST(AudioServiceListenerTest, OnInitWithAudioService_ProcessIdNotNull) {
  AudioServiceListener audio_service_listener(nullptr);
  service_manager::Identity audio_service_identity(audio::mojom::kServiceName);
  constexpr base::ProcessId pid(42);
  std::vector<RunningServiceInfoPtr> instances;
  instances.push_back(MakeTestServiceInfo(audio_service_identity, pid));
  audio_service_listener.OnInit(std::move(instances));
  EXPECT_EQ(pid, audio_service_listener.GetProcessId());
}

TEST(AudioServiceListenerTest, OnAudioServiceCreated_ProcessIdNotNull) {
  AudioServiceListener audio_service_listener(nullptr);
  service_manager::Identity audio_service_identity(audio::mojom::kServiceName);
  constexpr base::ProcessId pid(42);
  audio_service_listener.OnServiceStarted(audio_service_identity, pid);
  EXPECT_EQ(pid, audio_service_listener.GetProcessId());
}

TEST(AudioServiceListenerTest, OnAudioServiceStopped_ProcessIdNull) {
  AudioServiceListener audio_service_listener(nullptr);
  service_manager::Identity audio_service_identity(audio::mojom::kServiceName);
  constexpr base::ProcessId pid(42);
  audio_service_listener.OnServiceStarted(audio_service_identity, pid);
  EXPECT_EQ(pid, audio_service_listener.GetProcessId());
  audio_service_listener.OnServiceStopped(audio_service_identity);
  EXPECT_EQ(base::kNullProcessId, audio_service_listener.GetProcessId());
}

}  // namespace content
