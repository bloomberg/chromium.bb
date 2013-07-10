// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webinbandtexttrack_impl.h"

#include "base/logging.h"

namespace content {

WebInbandTextTrackImpl::WebInbandTextTrackImpl(
    Kind kind,
    const WebKit::WebString& label,
    const WebKit::WebString& language,
    int index)
    : client_(NULL),
      mode_(ModeDisabled),
      kind_(kind),
      label_(label),
      language_(language),
      index_(index) {
}

WebInbandTextTrackImpl::~WebInbandTextTrackImpl() {
  DCHECK(!client_);
}

void WebInbandTextTrackImpl::setClient(
    WebKit::WebInbandTextTrackClient* client) {
  client_ = client;
}

WebKit::WebInbandTextTrackClient* WebInbandTextTrackImpl::client() {
  return client_;
}

void WebInbandTextTrackImpl::setMode(Mode mode) {
  mode_ = mode;
}

WebInbandTextTrackImpl::Mode WebInbandTextTrackImpl::mode() const {
  return mode_;
}

WebInbandTextTrackImpl::Kind WebInbandTextTrackImpl::kind() const {
  return kind_;
}

bool WebInbandTextTrackImpl::isClosedCaptions() const {
  switch (kind_) {
  case KindCaptions:
  case KindSubtitles:
    return true;

  default:
    return false;
  }
}

WebKit::WebString WebInbandTextTrackImpl::label() const {
  return label_;
}

WebKit::WebString WebInbandTextTrackImpl::language() const {
  return language_;
}

bool WebInbandTextTrackImpl::isDefault() const {
  return false;
}

int WebInbandTextTrackImpl::textTrackIndex() const {
  return index_;
}

}  // namespace content
