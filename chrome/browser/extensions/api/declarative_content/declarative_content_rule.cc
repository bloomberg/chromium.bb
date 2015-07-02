// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_rule.h"

namespace extensions {

DeclarativeContentRule::DeclarativeContentRule(
    const Extension* extension,
    scoped_ptr<DeclarativeContentConditionSet> conditions,
    scoped_ptr<DeclarativeContentActionSet> actions,
    int priority)
    : extension(extension),
      conditions(conditions.Pass()),
      actions(actions.Pass()),
      priority(priority) {
}

DeclarativeContentRule::~DeclarativeContentRule() {}

}  // namespace extensions
