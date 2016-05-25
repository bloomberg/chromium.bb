// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/mock_permission_bubble_request.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/theme_resources.h"

MockPermissionBubbleRequest::MockPermissionBubbleRequest()
    : MockPermissionBubbleRequest("test",
                                  "button",
                                  "button",
                                  GURL("http://www.google.com"),
                                  PermissionBubbleType::UNKNOWN) {}

MockPermissionBubbleRequest::MockPermissionBubbleRequest(
    const std::string& text)
    : MockPermissionBubbleRequest(text,
                                  "button",
                                  "button",
                                  GURL("http://www.google.com"),
                                  PermissionBubbleType::UNKNOWN) {}

MockPermissionBubbleRequest::MockPermissionBubbleRequest(
    const std::string& text, PermissionBubbleType bubble_type)
    : MockPermissionBubbleRequest(text,
                                  "button",
                                  "button",
                                  GURL("http://www.google.com"),
                                  bubble_type) {}

MockPermissionBubbleRequest::MockPermissionBubbleRequest(
    const std::string& text,
    const GURL& url)
    : MockPermissionBubbleRequest(text,
                                  "button",
                                  "button",
                                  url,
                                  PermissionBubbleType::UNKNOWN) {}

MockPermissionBubbleRequest::MockPermissionBubbleRequest(
    const std::string& text,
    const std::string& accept_label,
    const std::string& deny_label)
    : MockPermissionBubbleRequest(text,
                                  accept_label,
                                  deny_label,
                                  GURL("http://www.google.com"),
                                  PermissionBubbleType::UNKNOWN) {}

MockPermissionBubbleRequest::~MockPermissionBubbleRequest() {}

int MockPermissionBubbleRequest::GetIconId() const {
  // Use a valid icon ID to support UI tests.
  return IDR_INFOBAR_MEDIA_STREAM_CAMERA;
}

base::string16 MockPermissionBubbleRequest::GetMessageTextFragment() const {
  return text_;
}

GURL MockPermissionBubbleRequest::GetOrigin() const {
  return origin_;
}

void MockPermissionBubbleRequest::PermissionGranted() {
  granted_ = true;
}

void MockPermissionBubbleRequest::PermissionDenied() {
  granted_ = false;
}

void MockPermissionBubbleRequest::Cancelled() {
  granted_ = false;
  cancelled_ = true;
}

void MockPermissionBubbleRequest::RequestFinished() {
  finished_ = true;
}

PermissionBubbleType MockPermissionBubbleRequest::GetPermissionBubbleType()
    const {
  return bubble_type_;
}

bool MockPermissionBubbleRequest::granted() {
  return granted_;
}

bool MockPermissionBubbleRequest::cancelled() {
  return cancelled_;
}

bool MockPermissionBubbleRequest::finished() {
  return finished_;
}

MockPermissionBubbleRequest::MockPermissionBubbleRequest(
    const std::string& text,
    const std::string& accept_label,
    const std::string& deny_label,
    const GURL& origin,
    PermissionBubbleType bubble_type)
    : granted_(false),
      cancelled_(false),
      finished_(false),
      bubble_type_(bubble_type) {
  text_ = base::UTF8ToUTF16(text);
  accept_label_ = base::UTF8ToUTF16(accept_label);
  deny_label_ = base::UTF8ToUTF16(deny_label);
  origin_ = origin.GetOrigin();
}
