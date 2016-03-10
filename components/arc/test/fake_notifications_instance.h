// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_NOTIFICATIONS_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_NOTIFICATIONS_INSTANCE_H_

#include "components/arc/common/notifications.mojom.h"
#include "components/arc/test/fake_arc_bridge_instance.h"

namespace arc {

class FakeNotificationsInstance : public NotificationsInstance {
 public:
  FakeNotificationsInstance(
      mojo::InterfaceRequest<NotificationsInstance> request);
  ~FakeNotificationsInstance() override;

  void Init(NotificationsHostPtr host_ptr) override;

  void SendNotificationEventToAndroid(const mojo::String& key,
                                      ArcNotificationEvent event) override;

  const std::vector<std::pair<mojo::String, ArcNotificationEvent>>& events()
      const;

  void WaitForIncomingMethodCall();

 private:
  std::vector<std::pair<mojo::String, ArcNotificationEvent>> events_;

  mojo::Binding<NotificationsInstance> binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeNotificationsInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_NOTIFICATIONS_INSTANCE_H_
