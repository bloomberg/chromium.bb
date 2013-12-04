// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "content/browser/aura/image_transport_factory.h"
#include "gpu/command_buffer/common/mailbox.h"

namespace content {

class GLHelper;

// This class holds a texture id and gpu::Mailbox, and deletes the texture
// id when the object itself is destroyed. Should only be created if a GLHelper
// exists on the ImageTransportFactory.
class OwnedMailbox : public base::RefCounted<OwnedMailbox>,
                     public ImageTransportFactoryObserver {
 public:
  explicit OwnedMailbox(GLHelper* gl_helper);

  uint32 texture_id() const { return texture_id_; }
  uint32 sync_point() const { return sync_point_; }
  const gpu::Mailbox& mailbox() const { return mailbox_; }

  void UpdateSyncPoint(uint32 sync_point);

 protected:
  virtual ~OwnedMailbox();

  virtual void OnLostResources() OVERRIDE;

 private:
  friend class base::RefCounted<OwnedMailbox>;

  uint32 texture_id_;
  gpu::Mailbox mailbox_;
  uint32 sync_point_;
  GLHelper* gl_helper_;
};

}  // namespace content
