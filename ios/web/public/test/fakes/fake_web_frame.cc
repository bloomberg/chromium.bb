// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/fakes/fake_web_frame.h"

#include "base/callback.h"

namespace web {
FakeWebFrame::FakeWebFrame(const std::string& frame_id,
                           bool is_main_frame,
                           GURL security_origin)
    : frame_id_(frame_id),
      is_main_frame_(is_main_frame),
      security_origin_(security_origin) {}

FakeWebFrame::~FakeWebFrame() {}

std::string FakeWebFrame::GetFrameId() const {
  return frame_id_;
}
bool FakeWebFrame::IsMainFrame() const {
  return is_main_frame_;
}
GURL FakeWebFrame::GetSecurityOrigin() const {
  return security_origin_;
}
bool FakeWebFrame::CanCallJavaScriptFunction() const {
  return false;
}

bool FakeWebFrame::CallJavaScriptFunction(
    const std::string& name,
    const std::vector<base::Value>& parameters) {
  return false;
}

bool FakeWebFrame::CallJavaScriptFunction(
    std::string name,
    const std::vector<base::Value>& parameters,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  return false;
}

}  // namespace web
