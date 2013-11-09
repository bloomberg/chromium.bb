// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/lib/message.h"

#include <stdlib.h>

#include <algorithm>

namespace mojo {

Message::Message()
    : data(NULL) {
}

Message::~Message() {
  free(data);
  std::for_each(handles.begin(), handles.end(), Close);
}

void Message::Swap(Message* other) {
  std::swap(data, other->data);
  std::swap(handles, other->handles);
}

}  // namespace mojo
