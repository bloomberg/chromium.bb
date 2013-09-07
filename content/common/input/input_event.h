// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_INPUT_EVENT_H_
#define CONTENT_COMMON_INPUT_INPUT_EVENT_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"

namespace content {

// An id'ed event that carries a specific input payload.
class CONTENT_EXPORT InputEvent {
 public:
  // Input data carried by the InputEvent.
  class CONTENT_EXPORT Payload {
   public:
    enum Type {
      IPC_MESSAGE,
      WEB_INPUT_EVENT,
      PAYLOAD_TYPE_MAX = WEB_INPUT_EVENT
    };
    virtual ~Payload() {}
    virtual Type GetType() const = 0;
  };

  // A valid InputEvent has a non-zero |id| and a non-NULL |payload|.
  static scoped_ptr<InputEvent> Create(int64 id, scoped_ptr<Payload> payload);

  template <typename PayloadType>
  static scoped_ptr<InputEvent> Create(int64 id,
                                       scoped_ptr<PayloadType> payload) {
    return Create(id, payload.template PassAs<Payload>());
  }

  InputEvent();
  virtual ~InputEvent();

  // Returns true if the initialized InputEvent is |valid()|, false otherwise.
  bool Initialize(int64 id, scoped_ptr<Payload> payload);

  int64 id() const { return id_; }
  const Payload* payload() const { return payload_.get(); }
  bool valid() const { return id() && payload(); }

 protected:
  InputEvent(int64 id, scoped_ptr<Payload> payload);

 private:
  int64 id_;
  scoped_ptr<Payload> payload_;

  DISALLOW_COPY_AND_ASSIGN(InputEvent);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_INPUT_EVENT_H_
