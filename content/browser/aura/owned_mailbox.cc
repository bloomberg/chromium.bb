// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aura/owned_mailbox.h"

#include "base/logging.h"
#include "content/browser/aura/image_transport_factory.h"
#include "content/common/gpu/client/gl_helper.h"

namespace content {

OwnedMailbox::OwnedMailbox(GLHelper* gl_helper)
    : texture_id_(0), sync_point_(0), gl_helper_(gl_helper) {
  texture_id_ = gl_helper_->CreateTexture();
  mailbox_ = gl_helper_->ProduceMailboxFromTexture(texture_id_, &sync_point_);
  ImageTransportFactory::GetInstance()->AddObserver(this);
}

OwnedMailbox::~OwnedMailbox() {
  ImageTransportFactory::GetInstance()->RemoveObserver(this);
  if (gl_helper_) {
    gl_helper_->WaitSyncPoint(sync_point_);
    gl_helper_->DeleteTexture(texture_id_);
  }
}

void OwnedMailbox::UpdateSyncPoint(uint32 sync_point) {
  if (sync_point)
    sync_point_ = sync_point;
}

void OwnedMailbox::OnLostResources() {
  gl_helper_->WaitSyncPoint(sync_point_);
  gl_helper_->DeleteTexture(texture_id_);
  texture_id_ = 0;
  mailbox_ = gpu::Mailbox();
  sync_point_ = 0;
  gl_helper_ = NULL;
}

}  // namespace content
