// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/mock_permission_bubble_request.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/theme_resources.h"

MockPermissionBubbleRequest::MockPermissionBubbleRequest()
    : granted_(false),
      cancelled_(false),
      finished_(false),
      user_gesture_(false) {
  text_ = base::ASCIIToUTF16("test");
  accept_label_ = base::ASCIIToUTF16("button");
  deny_label_ = base::ASCIIToUTF16("button");
  hostname_ = GURL("http://www.google.com");
}

MockPermissionBubbleRequest::MockPermissionBubbleRequest(
    const std::string& text)
    : granted_(false),
      cancelled_(false),
      finished_(false),
      user_gesture_(false) {
  text_ = base::UTF8ToUTF16(text);
  accept_label_ = base::ASCIIToUTF16("button");
  deny_label_ = base::ASCIIToUTF16("button");
  hostname_ = GURL("http://www.google.com");
}

MockPermissionBubbleRequest::MockPermissionBubbleRequest(
    const std::string& text,
    const GURL& url)
    : granted_(false),
      cancelled_(false),
      finished_(false),
      user_gesture_(false) {
  text_ = base::UTF8ToUTF16(text);
  accept_label_ = base::ASCIIToUTF16("button");
  deny_label_ = base::ASCIIToUTF16("button");
  hostname_ = url;
}

MockPermissionBubbleRequest::MockPermissionBubbleRequest(
    const std::string& text,
    const std::string& accept_label,
    const std::string& deny_label)
    : granted_(false),
      cancelled_(false),
      finished_(false),
      user_gesture_(false) {
  text_ = base::UTF8ToUTF16(text);
  accept_label_ = base::UTF8ToUTF16(accept_label);
  deny_label_ = base::UTF8ToUTF16(deny_label);
  hostname_ = GURL("http://www.google.com");
}

MockPermissionBubbleRequest::~MockPermissionBubbleRequest() {}

int MockPermissionBubbleRequest::GetIconID() const {
  // Use a valid icon ID to support UI tests.
  return IDR_INFOBAR_MEDIA_STREAM_CAMERA;
}

base::string16 MockPermissionBubbleRequest::GetMessageText() const {
  return text_;
}

base::string16 MockPermissionBubbleRequest::GetMessageTextFragment() const {
  return text_;
}

bool MockPermissionBubbleRequest::HasUserGesture() const {
  return user_gesture_;
}

GURL MockPermissionBubbleRequest::GetRequestingHostname() const {
  return hostname_;
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

bool MockPermissionBubbleRequest::granted() {
  return granted_;
}

bool MockPermissionBubbleRequest::cancelled() {
  return cancelled_;
}

bool MockPermissionBubbleRequest::finished() {
  return finished_;
}

void MockPermissionBubbleRequest::SetHasUserGesture() {
  user_gesture_ = true;
}
