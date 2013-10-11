// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/lib/message.h"

#include <stdlib.h>

namespace mojo {

Message::Message()
    : data(NULL) {
}

Message::~Message() {
}

}  // namespace mojo
