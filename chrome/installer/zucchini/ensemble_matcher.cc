// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/ensemble_matcher.h"

#include <limits>
#include <utility>

#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace zucchini {

/******** EnsembleMatcher ********/

EnsembleMatcher::EnsembleMatcher() = default;

EnsembleMatcher::~EnsembleMatcher() = default;

void EnsembleMatcher::Trim() {
  // TODO(huangs): Add MultiDex handling logic when we add DEX support.
}

void EnsembleMatcher::GenerateSeparators(ConstBufferView new_image) {
  ConstBufferView::iterator it = new_image.begin();
  for (ElementMatch& match : matches_) {
    ConstBufferView new_sub_image(new_image[match.new_element.region()]);
    separators_.push_back(
        ConstBufferView::FromRange(it, new_sub_image.begin()));
    it = new_sub_image.end();
  }
  separators_.push_back(ConstBufferView::FromRange(it, new_image.end()));
}

}  // namespace zucchini
