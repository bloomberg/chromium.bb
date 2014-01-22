// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/gcm_client.h"

namespace gcm {

GCMClient::OutgoingMessage::OutgoingMessage()
    : time_to_live(0) {
}

GCMClient::OutgoingMessage::~OutgoingMessage() {
}

GCMClient::IncomingMessage::IncomingMessage() {
}

GCMClient::IncomingMessage::~IncomingMessage() {
}

GCMClient::GCMClient() {
}

GCMClient::~GCMClient() {
}

}  // namespace gcm
