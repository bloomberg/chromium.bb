// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STATE_MATRIX_H_
#define MEDIA_BASE_STATE_MATRIX_H_

#include <map>

#include "base/logging.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT StateMatrix {
 public:
  StateMatrix();
  ~StateMatrix();

  template <typename HandlerType>
  void AddState(int state, int (HandlerType::* handler)()) {
    CHECK(!IsStateDefined(state));
    StateEntryBase* entry = new StateEntry<HandlerType>(handler);
    states_.insert(std::make_pair(state, entry));
  }

  bool IsStateDefined(int state);

  int ExecuteHandler(void* instance, int state);

 private:
  class StateEntryBase {
   public:
    StateEntryBase() {}
    virtual ~StateEntryBase() {}

    virtual int ExecuteHandler(void* instance) = 0;
  };

  template <typename HandlerType>
  class StateEntry : public StateEntryBase {
   public:
    explicit StateEntry(int (HandlerType::* handler)()) : handler_(handler) {}
    virtual ~StateEntry() {}

    virtual int ExecuteHandler(void* instance) {
      return (reinterpret_cast<HandlerType*>(instance)->*handler_)();
    }

   private:
    int (HandlerType::* handler_)();
  };

  typedef std::map<int, StateEntryBase*> StateMap;
  StateMap states_;

  DISALLOW_COPY_AND_ASSIGN(StateMatrix);
};

}  // namespace media

#endif  // MEDIA_BASE_STATE_MATRIX_H_
