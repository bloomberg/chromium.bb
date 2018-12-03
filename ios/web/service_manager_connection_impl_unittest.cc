// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/service_manager_connection_impl.h"

#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread.h"
#include "services/service_manager/public/cpp/constants.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {
namespace {

constexpr char kTestServiceName[] = "test service";

}  // namespace

using ServiceManagerConnectionImplTest = PlatformTest;

TEST_F(ServiceManagerConnectionImplTest, ServiceLaunch) {
  TestWebThreadBundle thread_bundle(
      TestWebThreadBundle::Options::REAL_IO_THREAD);
  service_manager::mojom::ServicePtr service;
  ServiceManagerConnectionImpl connection_impl(
      mojo::MakeRequest(&service),
      base::CreateSingleThreadTaskRunnerWithTraits({WebThread::IO}));
  ServiceManagerConnection& connection = connection_impl;
  base::RunLoop run_loop;
  connection.SetDefaultServiceRequestHandler(base::BindLambdaForTesting(
      [&run_loop](const std::string& service_name,
                  service_manager::mojom::ServiceRequest request) {
        EXPECT_EQ(kTestServiceName, service_name);
        run_loop.Quit();
      }));
  connection.Start();
  service_manager::BindSourceInfo source_info(
      service_manager::Identity{service_manager::mojom::kServiceName,
                                service_manager::kSystemInstanceGroup,
                                base::Token{}, base::Token::CreateRandom()},
      service_manager::CapabilitySet());
  service_manager::mojom::ServiceFactoryPtr factory;
  service->OnBindInterface(
      source_info, service_manager::mojom::ServiceFactory::Name_,
      mojo::MakeRequest(&factory).PassMessagePipe(), base::DoNothing());
  service_manager::mojom::ServicePtr created_service;
  service_manager::mojom::PIDReceiverPtr pid_receiver;
  mojo::MakeRequest(&pid_receiver);
  factory->CreateService(mojo::MakeRequest(&created_service), kTestServiceName,
                         std::move(pid_receiver));
  run_loop.Run();
}

}  // namespace web
