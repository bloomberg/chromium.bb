// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_rule.h"

namespace extensions {

DeclarativeContentRule::DeclarativeContentRule()
    : extension(nullptr),
      priority(0) {
}

DeclarativeContentRule::~DeclarativeContentRule() {}

}  // namespace extensions
