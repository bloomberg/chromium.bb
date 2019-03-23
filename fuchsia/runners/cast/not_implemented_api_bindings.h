// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_NOT_IMPLEMENTED_API_BINDINGS_H_
#define FUCHSIA_RUNNERS_CAST_NOT_IMPLEMENTED_API_BINDINGS_H_

namespace chromium {
namespace web {
class Frame;
}
}  // namespace chromium

// Configures |frame| to inject placeholder "stub" scripts on every page load.
void InjectNotImplementedApiBindings(chromium::web::Frame* frame);

#endif  // FUCHSIA_RUNNERS_CAST_NOT_IMPLEMENTED_API_BINDINGS_H_
