// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/ensemble.h"

#include "base/basictypes.h"
#include "base/string_number_conversions.h"

#include "courgette/image_info.h"
#include "courgette/region.h"
#include "courgette/streams.h"
#include "courgette/simple_delta.h"

namespace courgette {

Element::Element(ExecutableType kind,
                 Ensemble* ensemble,
                 const Region& region,
                 PEInfo* info)
    : kind_(kind), ensemble_(ensemble), region_(region), info_(info) {
}

Element::~Element() {
  delete info_;
}

std::string Element::Name() const {
  return ensemble_->name() + "("
      + base::IntToString(kind()) + ","
      + base::Uint64ToString(offset_in_ensemble()) + ","
      + base::Uint64ToString(region().length()) + ")";
}

// Scans the Ensemble's region, sniffing out Elements.  We assume that the
// elements do not overlap.
Status Ensemble::FindEmbeddedElements() {

  size_t length = region_.length();
  const uint8* start = region_.start();

  size_t position = 0;
  while (position < length) {
    ExecutableType type = DetectExecutableType(start + position,
                                               length - position);

    //
    // TODO(dgarrett) This switch can go away totally after two things.
    //
    // Make ImageInfo generic for all executable types.
    // Find a generic way to handle length detection for executables.
    //
    // When this switch is gone, that's one less piece of code that is
    // executable type aware.
    //
    switch (type) {
      case UNKNOWN: {
        // No Element found at current position.
        ++position;
        break;
      }
      case WIN32_X86: {
        // The Info is only created to detect the length of the executable
        courgette::PEInfo* info(new courgette::PEInfo());
        info->Init(start + position, length - position);
        if (!info->ParseHeader()) {
          delete info;
          position++;
          break;
        }
        Region region(start + position, info->length());

        Element* element = new Element(type, this, region, info);
        owned_elements_.push_back(element);
        elements_.push_back(element);
        position += region.length();
        break;
      }
    }
  }
  return C_OK;
}

Ensemble::~Ensemble() {
  for (size_t i = 0;  i < owned_elements_.size();  ++i)
    delete owned_elements_[i];
}

}  // namespace
