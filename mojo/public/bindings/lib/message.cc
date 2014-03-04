// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/message.h"

#include <assert.h>
#include <stdlib.h>

#include <algorithm>

namespace mojo {

Message::Message()
    : data_(NULL) {
}

Message::~Message() {
  free(data_);

  for (std::vector<Handle>::iterator it = handles_.begin();
       it != handles_.end(); ++it) {
    if (it->is_valid())
      CloseRaw(*it);
  }
}

void Message::AllocData(uint32_t num_bytes) {
  assert(!data_);
  data_ = static_cast<MessageData*>(malloc(num_bytes));
}

void Message::AdoptData(MessageData* data) {
  assert(!data_);
  data_ = data;
}

void Message::Swap(Message* other) {
  std::swap(data_, other->data_);
  std::swap(handles_, other->handles_);
}

}  // namespace mojo
