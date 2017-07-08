// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_LOG_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_LOG_H_

#include <memory>

#include "base/macros.h"
#include "media/base/media_log.h"
#include "media/mojo/interfaces/media_log.mojom.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"

namespace media {

class MojoMediaLog final : public MediaLog {
 public:
  // TODO(sandersd): Template on Ptr type to support non-associated.
  explicit MojoMediaLog(
      mojo::AssociatedInterfacePtr<mojom::MediaLog> remote_media_log);
  ~MojoMediaLog() final;

  // MediaLog implementation.
  void AddEvent(std::unique_ptr<MediaLogEvent> event) override;

 private:
  mojo::AssociatedInterfacePtr<mojom::MediaLog> remote_media_log_;

  DISALLOW_COPY_AND_ASSIGN(MojoMediaLog);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_LOG_H_
