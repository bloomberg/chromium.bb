// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PRESENTATION_PRESENTATION_SESSION_DISPATCHER_H_
#define CONTENT_RENDERER_PRESENTATION_PRESENTATION_SESSION_DISPATCHER_H_

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/common/presentation/presentation_service.mojom.h"

namespace content {

// A wrapper around the Mojo PresentationSession remote object.
class CONTENT_EXPORT PresentationSessionDispatcher {
 public:
  explicit PresentationSessionDispatcher(
      presentation::PresentationSessionInfoPtr session_info);
  ~PresentationSessionDispatcher();

  const mojo::String& GetUrl() const;
  const mojo::String& GetId() const;

 private:
  presentation::PresentationSessionInfoPtr session_info_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PRESENTATION_PRESENTATION_SESSION_DISPATCHER_H_
