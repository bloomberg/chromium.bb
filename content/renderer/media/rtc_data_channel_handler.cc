// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_data_channel_handler.h"

#include <string>

#include "base/logging.h"
#include "base/utf_string_conversions.h"

namespace content {

RtcDataChannelHandler::RtcDataChannelHandler(
    webrtc::DataChannelInterface* channel)
    : channel_(channel),
      webkit_client_(NULL) {
  DVLOG(1) << "::ctor";
  channel_->RegisterObserver(this);
}

RtcDataChannelHandler::~RtcDataChannelHandler() {
  DVLOG(1) << "::dtor";
  channel_->UnregisterObserver();
}

void RtcDataChannelHandler::setClient(
    WebKit::WebRTCDataChannelHandlerClient* client) {
  webkit_client_ = client;
}

WebKit::WebString RtcDataChannelHandler::label() {
  return UTF8ToUTF16(channel_->label());
}

bool RtcDataChannelHandler::isReliable() {
  return channel_->reliable();
}

unsigned long RtcDataChannelHandler::bufferedAmount() {
  return channel_->buffered_amount();
}

bool RtcDataChannelHandler::sendStringData(const WebKit::WebString& data) {
  std::string utf8_buffer = UTF16ToUTF8(data);
  talk_base::Buffer buffer(utf8_buffer.c_str(), utf8_buffer.length());
  webrtc::DataBuffer data_buffer(buffer, false);
  return channel_->Send(data_buffer);
}

bool RtcDataChannelHandler::sendRawData(const char* data, size_t length) {
  talk_base::Buffer buffer(data, length);
  webrtc::DataBuffer data_buffer(buffer, true);
  return channel_->Send(data_buffer);
}

void RtcDataChannelHandler::close() {
  channel_->Close();
}

void RtcDataChannelHandler::OnStateChange() {
  if (!webkit_client_) {
    LOG(ERROR) << "WebRTCDataChannelHandlerClient not set.";
    return;
  }
  DVLOG(1) << "OnStateChange " << channel_->state();
  switch (channel_->state()) {
    case webrtc::DataChannelInterface::kConnecting:
      webkit_client_->didChangeReadyState(
          WebKit::WebRTCDataChannelHandlerClient::ReadyStateConnecting);
      break;
    case webrtc::DataChannelInterface::kOpen:
      webkit_client_->didChangeReadyState(
          WebKit::WebRTCDataChannelHandlerClient::ReadyStateOpen);
      break;
    case webrtc::DataChannelInterface::kClosing:
      webkit_client_->didChangeReadyState(
          WebKit::WebRTCDataChannelHandlerClient::ReadyStateClosing);
      break;
    case webrtc::DataChannelInterface::kClosed:
      webkit_client_->didChangeReadyState(
          WebKit::WebRTCDataChannelHandlerClient::ReadyStateClosed);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void RtcDataChannelHandler::OnMessage(const webrtc::DataBuffer& buffer) {
  if (!webkit_client_) {
    LOG(ERROR) << "WebRTCDataChannelHandlerClient not set.";
    return;
  }

  if (buffer.binary) {
    webkit_client_->didReceiveRawData(buffer.data.data(), buffer.data.length());
  } else {
    string16 utf16;
    if (!UTF8ToUTF16(buffer.data.data(), buffer.data.length(), &utf16)) {
      LOG(ERROR) << "Failed convert received data to UTF16";
      return;
    }
    webkit_client_->didReceiveStringData(utf16);
  }
}

}  // namespace content
