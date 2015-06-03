// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_SERVICE_PROVIDER_H_
#define MEDIA_MOJO_SERVICES_MEDIA_SERVICE_PROVIDER_H_

#include "media/base/media_export.h"
#include "media/base/renderer_factory.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "media/mojo/interfaces/media_renderer.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_ptr.h"

namespace media {

// A class that provides mojo media services. It helps hide implementation
// details on how these services are provided.
class MediaServiceProvider {
 public:
  MediaServiceProvider();
  virtual ~MediaServiceProvider();

  virtual void ConnectToService(
      mojo::InterfacePtr<mojo::MediaRenderer>* media_renderer_ptr) = 0;

  virtual void ConnectToService(
      mojo::InterfacePtr<mojo::ContentDecryptionModule>* cdm_ptr) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaServiceProvider);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_SERVICE_PROVIDER_H_
