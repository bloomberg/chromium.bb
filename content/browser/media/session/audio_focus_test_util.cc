// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/audio_focus_test_util.h"

namespace content {
namespace test {

namespace {

void ReceivedSessionInfo(media_session::mojom::MediaSessionInfoPtr* info_out,
                         base::RepeatingClosure callback,
                         media_session::mojom::MediaSessionInfoPtr result) {
  *info_out = std::move(result);
  std::move(callback).Run();
}

}  // namespace

TestAudioFocusObserver::TestAudioFocusObserver() {
  RegisterAudioFocusObserver();
}

TestAudioFocusObserver::~TestAudioFocusObserver() = default;

void TestAudioFocusObserver::OnFocusGained(
    media_session::mojom::MediaSessionInfoPtr session,
    media_session::mojom::AudioFocusType type) {
  focus_gained_type_ = type;
  focus_gained_session_ = std::move(session);

  if (wait_for_gained_)
    run_loop_.Quit();
}

void TestAudioFocusObserver::OnFocusLost(
    media_session::mojom::MediaSessionInfoPtr session) {
  focus_lost_session_ = std::move(session);

  if (wait_for_lost_)
    run_loop_.Quit();
}

void TestAudioFocusObserver::WaitForGainedEvent() {
  if (!focus_gained_session_.is_null())
    return;

  wait_for_gained_ = true;
  run_loop_.Run();
}

void TestAudioFocusObserver::WaitForLostEvent() {
  if (!focus_lost_session_.is_null())
    return;

  wait_for_lost_ = true;
  run_loop_.Run();
}

media_session::mojom::MediaSessionInfoPtr GetMediaSessionInfoSync(
    media_session::mojom::MediaSession* session) {
  media_session::mojom::MediaSessionInfoPtr session_info;
  base::RunLoop run_loop;

  session->GetMediaSessionInfo(base::BindOnce(
      &ReceivedSessionInfo, &session_info, run_loop.QuitClosure()));

  return session_info;
}

}  // namespace test
}  // namespace content
