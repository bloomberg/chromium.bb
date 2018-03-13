// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ZUCCHINI_TOOLS_H_
#define COMPONENTS_ZUCCHINI_ZUCCHINI_TOOLS_H_

#include <iosfwd>
#include <vector>

#include "components/zucchini/buffer_view.h"
#include "components/zucchini/zucchini.h"

namespace zucchini {

// The functions below are called to print diagnosis information, so outputs are
// printed using std::ostream instead of LOG().

// Prints stats on references found in |image|. If |do_dump| is true, then
// prints all references (locations and targets).
status::Code ReadReferences(ConstBufferView image,
                            bool do_dump,
                            std::ostream& out);

// Prints regions and types of all detected executables in |image|. Appends
// detected subregions to |sub_image_list|.
status::Code DetectAll(ConstBufferView image,
                       std::ostream& out,
                       std::vector<ConstBufferView>* sub_image_list);

// Prints all matched regions from |old_image| to |new_image|.
status::Code MatchAll(ConstBufferView old_image,
                      ConstBufferView new_image,
                      std::ostream& out);

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_ZUCCHINI_TOOLS_H_
