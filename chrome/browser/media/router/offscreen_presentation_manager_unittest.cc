// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/stl_util.h"
#include "chrome/browser/media/router/offscreen_presentation_manager.h"
#include "chrome/browser/media/router/test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;

namespace media_router {

namespace {
const char kPresentationId[] = "presentationId";
const char kPresentationId2[] = "presentationId2";
const char kPresentationUrl[] = "http://www.example.com/presentation.html";
}  // namespace

class MockReceiverConnectionAvailableCallback {
 public:
  void OnReceiverConnectionAvailable(
      const content::PresentationInfo& presentation_info,
      content::PresentationConnectionPtr controller_conn,
      content::PresentationConnectionRequest receiver_conn_request) {
    OnReceiverConnectionAvailableRaw(presentation_info, controller_conn.get());
  }

  MOCK_METHOD2(OnReceiverConnectionAvailableRaw,
               void(const content::PresentationInfo&,
                    blink::mojom::PresentationConnection*));
};

class OffscreenPresentationManagerTest : public ::testing::Test {
 public:
  OffscreenPresentationManagerTest()
      : render_frame_host_id_(1, 1),
        presentation_info_(GURL(kPresentationUrl), kPresentationId),
        route_("route_1",
               MediaSource("source_1"),
               "sink_1",
               "",
               false,
               "",
               false) {}

  OffscreenPresentationManager* manager() { return &manager_; }

  void VerifyPresentationsSize(size_t expected) {
    EXPECT_EQ(expected, manager_.offscreen_presentations_.size());
  }

  void VerifyControllerSize(size_t expected,
                            const std::string& presentationId) {
    EXPECT_TRUE(
        base::ContainsKey(manager_.offscreen_presentations_, presentationId));
    EXPECT_EQ(expected, manager_.offscreen_presentations_[presentationId]
                            ->pending_controllers_.size());
  }

  void RegisterController(const std::string& presentation_id,
                          content::PresentationConnectionPtr controller) {
    RegisterController(
        content::PresentationInfo(GURL(kPresentationUrl), presentation_id),
        render_frame_host_id_, std::move(controller));
  }

  void RegisterController(const RenderFrameHostId& render_frame_id,
                          content::PresentationConnectionPtr controller) {
    RegisterController(presentation_info_, render_frame_id,
                       std::move(controller));
  }

  void RegisterController(content::PresentationConnectionPtr controller) {
    RegisterController(presentation_info_, render_frame_host_id_,
                       std::move(controller));
  }

  void RegisterController(const content::PresentationInfo& presentation_info,
                          const RenderFrameHostId& render_frame_id,
                          content::PresentationConnectionPtr controller) {
    content::PresentationConnectionRequest receiver_conn_request;
    manager()->RegisterOffscreenPresentationController(
        presentation_info, render_frame_id, std::move(controller),
        std::move(receiver_conn_request), route_);
  }

  void RegisterReceiver(
      MockReceiverConnectionAvailableCallback& receiver_callback) {
    RegisterReceiver(kPresentationId, receiver_callback);
  }

  void RegisterReceiver(
      const std::string& presentation_id,
      MockReceiverConnectionAvailableCallback& receiver_callback) {
    manager()->OnOffscreenPresentationReceiverCreated(
        content::PresentationInfo(GURL(kPresentationUrl), presentation_id),
        base::Bind(&MockReceiverConnectionAvailableCallback::
                       OnReceiverConnectionAvailable,
                   base::Unretained(&receiver_callback)));
  }

  void UnregisterController(const RenderFrameHostId& render_frame_id) {
    manager()->UnregisterOffscreenPresentationController(kPresentationId,
                                                         render_frame_id);
  }

  void UnregisterController() {
    manager()->UnregisterOffscreenPresentationController(kPresentationId,
                                                         render_frame_host_id_);
  }

  void UnregisterReceiver() {
    manager()->OnOffscreenPresentationReceiverTerminated(kPresentationId);
  }

 private:
  const RenderFrameHostId render_frame_host_id_;
  const content::PresentationInfo presentation_info_;
  OffscreenPresentationManager manager_;
  MediaRoute route_;
};

TEST_F(OffscreenPresentationManagerTest, RegisterUnregisterController) {
  content::PresentationConnectionPtr controller;
  RegisterController(std::move(controller));
  VerifyPresentationsSize(1);
  UnregisterController();
  VerifyPresentationsSize(0);
}

TEST_F(OffscreenPresentationManagerTest, RegisterUnregisterReceiver) {
  MockReceiverConnectionAvailableCallback receiver_callback;
  RegisterReceiver(receiver_callback);
  VerifyPresentationsSize(1);
  UnregisterReceiver();
  VerifyPresentationsSize(0);
}

TEST_F(OffscreenPresentationManagerTest, UnregisterNonexistentController) {
  UnregisterController();
  VerifyPresentationsSize(0);
}

TEST_F(OffscreenPresentationManagerTest, UnregisterNonexistentReceiver) {
  UnregisterReceiver();
  VerifyPresentationsSize(0);
}

TEST_F(OffscreenPresentationManagerTest,
       RegisterMultipleControllersSamePresentation) {
  content::PresentationConnectionPtr controller1;
  RegisterController(RenderFrameHostId(1, 1), std::move(controller1));
  content::PresentationConnectionPtr controller2;
  RegisterController(RenderFrameHostId(1, 2), std::move(controller2));
  VerifyPresentationsSize(1);
}

TEST_F(OffscreenPresentationManagerTest,
       RegisterMultipleControllersDifferentPresentations) {
  content::PresentationConnectionPtr controller1;
  RegisterController(kPresentationId, std::move(controller1));
  content::PresentationConnectionPtr controller2;
  RegisterController(kPresentationId2, std::move(controller2));
  VerifyPresentationsSize(2);
}

TEST_F(OffscreenPresentationManagerTest,
       RegisterControllerThenReceiverInvokesCallback) {
  content::PresentationConnectionPtr controller;
  MockReceiverConnectionAvailableCallback receiver_callback;

  VerifyPresentationsSize(0);

  RegisterController(std::move(controller));
  EXPECT_CALL(receiver_callback, OnReceiverConnectionAvailableRaw(_, _));
  RegisterReceiver(receiver_callback);
}

TEST_F(OffscreenPresentationManagerTest,
       UnregisterReceiverFromConnectedPresentation) {
  content::PresentationConnectionPtr controller;
  MockReceiverConnectionAvailableCallback receiver_callback;

  VerifyPresentationsSize(0);

  RegisterController(std::move(controller));
  EXPECT_CALL(receiver_callback, OnReceiverConnectionAvailableRaw(_, _));
  RegisterReceiver(receiver_callback);
  UnregisterReceiver();

  VerifyPresentationsSize(0);
}

TEST_F(OffscreenPresentationManagerTest,
       UnregisterControllerFromConnectedPresentation) {
  content::PresentationConnectionPtr controller;
  MockReceiverConnectionAvailableCallback receiver_callback;

  VerifyPresentationsSize(0);

  RegisterController(std::move(controller));
  EXPECT_CALL(receiver_callback, OnReceiverConnectionAvailableRaw(_, _));
  RegisterReceiver(receiver_callback);
  UnregisterController();

  VerifyPresentationsSize(1);
}

TEST_F(OffscreenPresentationManagerTest,
       UnregisterReceiverThenControllerFromConnectedPresentation) {
  content::PresentationConnectionPtr controller;
  MockReceiverConnectionAvailableCallback receiver_callback;

  VerifyPresentationsSize(0);

  RegisterController(std::move(controller));
  EXPECT_CALL(receiver_callback, OnReceiverConnectionAvailableRaw(_, _));
  RegisterReceiver(receiver_callback);
  UnregisterReceiver();
  UnregisterController();

  VerifyPresentationsSize(0);
}

TEST_F(OffscreenPresentationManagerTest,
       UnregisterControllerThenReceiverFromConnectedPresentation) {
  content::PresentationConnectionPtr controller;
  MockReceiverConnectionAvailableCallback receiver_callback;

  VerifyPresentationsSize(0);

  RegisterController(std::move(controller));
  EXPECT_CALL(receiver_callback, OnReceiverConnectionAvailableRaw(_, _));
  RegisterReceiver(receiver_callback);
  UnregisterController();
  UnregisterReceiver();

  VerifyPresentationsSize(0);
}

TEST_F(OffscreenPresentationManagerTest,
       RegisterTwoControllersThenReceiverInvokesCallbackTwice) {
  content::PresentationConnectionPtr controller1;
  RegisterController(RenderFrameHostId(1, 1), std::move(controller1));
  content::PresentationConnectionPtr controller2;
  RegisterController(RenderFrameHostId(1, 2), std::move(controller2));

  MockReceiverConnectionAvailableCallback receiver_callback;
  EXPECT_CALL(receiver_callback, OnReceiverConnectionAvailableRaw(_, _))
      .Times(2);
  RegisterReceiver(receiver_callback);
}

TEST_F(OffscreenPresentationManagerTest,
       RegisterControllerReceiverConontrollerInvokesCallbackTwice) {
  content::PresentationConnectionPtr controller1;
  RegisterController(RenderFrameHostId(1, 1), std::move(controller1));

  MockReceiverConnectionAvailableCallback receiver_callback;
  EXPECT_CALL(receiver_callback, OnReceiverConnectionAvailableRaw(_, _))
      .Times(2);
  RegisterReceiver(receiver_callback);

  content::PresentationConnectionPtr controller2;
  RegisterController(RenderFrameHostId(1, 2), std::move(controller2));
}

TEST_F(OffscreenPresentationManagerTest,
       UnregisterFirstControllerFromeConnectedPresentation) {
  content::PresentationConnectionPtr controller1;
  RegisterController(RenderFrameHostId(1, 1), std::move(controller1));
  content::PresentationConnectionPtr controller2;
  RegisterController(RenderFrameHostId(1, 2), std::move(controller2));

  MockReceiverConnectionAvailableCallback receiver_callback;
  EXPECT_CALL(receiver_callback, OnReceiverConnectionAvailableRaw(_, _))
      .Times(2);
  RegisterReceiver(receiver_callback);
  UnregisterController(RenderFrameHostId(1, 1));
  UnregisterController(RenderFrameHostId(1, 1));

  VerifyPresentationsSize(1);
}

TEST_F(OffscreenPresentationManagerTest, TwoPresentations) {
  content::PresentationConnectionPtr controller1;
  RegisterController(kPresentationId, std::move(controller1));

  MockReceiverConnectionAvailableCallback receiver_callback1;
  EXPECT_CALL(receiver_callback1, OnReceiverConnectionAvailableRaw(_, _))
      .Times(1);
  RegisterReceiver(kPresentationId, receiver_callback1);

  content::PresentationConnectionPtr controller2;
  RegisterController(kPresentationId2, std::move(controller2));

  MockReceiverConnectionAvailableCallback receiver_callback2;
  EXPECT_CALL(receiver_callback2, OnReceiverConnectionAvailableRaw(_, _))
      .Times(1);
  RegisterReceiver(kPresentationId2, receiver_callback2);

  VerifyPresentationsSize(2);

  UnregisterReceiver();
  VerifyPresentationsSize(1);
}

TEST_F(OffscreenPresentationManagerTest, TestIsOffscreenPresentation) {
  EXPECT_FALSE(manager()->IsOffscreenPresentation(kPresentationId));
  content::PresentationConnectionPtr controller1;
  RegisterController(kPresentationId, std::move(controller1));
  EXPECT_TRUE(manager()->IsOffscreenPresentation(kPresentationId));
}

TEST_F(OffscreenPresentationManagerTest, TestRegisterAndGetRoute) {
  MediaSource source("source_1");
  MediaRoute route("route_1", source, "sink_1", "", false, "", false);

  EXPECT_FALSE(manager()->GetRoute(kPresentationId));
  content::PresentationConnectionPtr controller;
  RegisterController(std::move(controller));

  auto* actual_route = manager()->GetRoute(kPresentationId);
  EXPECT_TRUE(actual_route);
  EXPECT_TRUE(route.Equals(*actual_route));
}

}  // namespace media_router
