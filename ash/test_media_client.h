// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_MEDIA_CLIENT_H_
#define ASH_TEST_MEDIA_CLIENT_H_

#include "ash/public/interfaces/media.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace ash {

// Implement MediaClient mojo interface to simulate chrome behavior in tests.
// This breaks the ash/chrome dependency to allow testing ash code in isolation.
class TestMediaClient : public mojom::MediaClient {
 public:
  TestMediaClient();
  ~TestMediaClient() override;

  mojom::MediaClientAssociatedPtrInfo CreateAssociatedPtrInfo();

  // mojom::MediaClient:
  void HandleMediaNextTrack() override;
  void HandleMediaPlayPause() override;
  void HandleMediaPrevTrack() override;
  void RequestCaptureState() override;
  void SuspendMediaSessions() override;

  bool media_sessions_suspended() const { return media_sessions_suspended_; }

 private:
  bool media_sessions_suspended_ = false;

  mojo::AssociatedBinding<mojom::MediaClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(TestMediaClient);
};

}  // namespace ash

#endif  // ASH_TEST_MEDIA_CLIENT_H_
