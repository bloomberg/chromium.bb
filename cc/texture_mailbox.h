// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEXTURE_MAILBOX_H_
#define CC_TEXTURE_MAILBOX_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "cc/cc_export.h"
#include "cc/transferable_resource.h"

namespace cc {

class CC_EXPORT TextureMailbox {
 public:
  typedef base::Callback<void(unsigned)> ReleaseCallback;
  TextureMailbox();
  TextureMailbox(const std::string& mailbox_name,
                 const ReleaseCallback& callback);
  TextureMailbox(const Mailbox& mailbox_name,
                 const ReleaseCallback& callback);
  TextureMailbox(const Mailbox& mailbox_name,
                 const ReleaseCallback& callback,
                 unsigned sync_point);

  ~TextureMailbox();

  const ReleaseCallback& callback() const { return callback_; }
  const int8* data() const { return name_.name; }
  bool Equals(const Mailbox&) const;
  bool Equals(const TextureMailbox&) const;
  bool IsEmpty() const;
  const Mailbox& name() const { return name_; }
  void ResetSyncPoint() { sync_point_ = 0; }
  void RunReleaseCallback(unsigned sync_point) const;
  void SetName(const Mailbox&);
  unsigned sync_point() const { return sync_point_; }

 private:
  Mailbox name_;
  ReleaseCallback callback_;
  unsigned sync_point_;
};

}  // namespace cc

#endif  // CC_TEXTURE_MAILBOX_H_
