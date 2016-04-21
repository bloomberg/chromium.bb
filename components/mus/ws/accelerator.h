// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_ACCELERATOR_H_
#define COMPONENTS_MUS_WS_ACCELERATOR_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/mus/public/interfaces/input_event_matcher.mojom.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect_f.h"

namespace mus {
namespace ws {

// An Accelerator encompasses an id defined by the client, along with a unique
// mojom::EventMatcher. See WindowManagerClient.
//
// This provides a WeakPtr, as the client might delete the accelerator between
// an event having been matched and the dispatch of the accelerator to the
// client.
class Accelerator {
 public:
  Accelerator(uint32_t id, const mojom::EventMatcher& matcher);
  ~Accelerator();

  // Returns true if |event| and |phase | matches the definition in the
  // mojom::EventMatcher used for initialization.
  bool MatchesEvent(const ui::Event& event,
                    const mojom::AcceleratorPhase phase) const;

  // Returns true if |other| was created with an identical mojom::EventMatcher.
  bool EqualEventMatcher(const Accelerator* other) const;

  base::WeakPtr<Accelerator> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

  uint32_t id() const { return id_; }

 private:
  enum MatchFields {
    NONE = 0,
    TYPE = 1 << 0,
    FLAGS = 1 << 1,
    KEYBOARD_CODE = 1 << 2,
    POINTER_KIND = 1 << 3,
    POINTER_LOCATION = 1 << 4,
  };

  uint32_t id_;
  uint32_t fields_to_match_;
  mojom::AcceleratorPhase accelerator_phase_;
  ui::EventType event_type_;
  // Bitfields of kEventFlag* and kMouseEventFlag* values in
  // input_event_constants.mojom.
  int event_flags_;
  int ignore_event_flags_;
  uint16_t keyboard_code_;
  ui::EventPointerType pointer_type_;
  gfx::RectF pointer_region_;

  base::WeakPtrFactory<Accelerator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Accelerator);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_ACCELERATOR_H_
