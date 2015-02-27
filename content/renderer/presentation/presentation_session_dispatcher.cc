// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/presentation/presentation_session_dispatcher.h"

#include "base/logging.h"

namespace content {

PresentationSessionDispatcher::PresentationSessionDispatcher(
    presentation::PresentationSessionInfoPtr session_info)
    : session_info_(session_info.Pass()) {
}

PresentationSessionDispatcher::~PresentationSessionDispatcher() {
}

const mojo::String& PresentationSessionDispatcher::GetUrl() const {
  DCHECK(!session_info_.is_null());
  return session_info_->url;
}

const mojo::String& PresentationSessionDispatcher::GetId() const {
  DCHECK(!session_info_.is_null());
  return session_info_->id;
}

}  // namespace content
