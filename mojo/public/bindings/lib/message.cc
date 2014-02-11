// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/message.h"

#include <stdlib.h>

#include <algorithm>

namespace mojo {

Message::Message()
    : data(NULL) {
}

Message::~Message() {
  free(data);

  for (std::vector<Handle>::iterator it = handles.begin(); it != handles.end();
       ++it) {
    if (it->is_valid())
      CloseRaw(*it);
  }
}

void Message::Swap(Message* other) {
  std::swap(data, other->data);
  std::swap(handles, other->handles);
}

}  // namespace mojo
