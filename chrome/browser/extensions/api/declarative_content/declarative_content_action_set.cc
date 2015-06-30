// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/declarative_content_action_set.h"

namespace extensions {

DeclarativeContentActionSet::DeclarativeContentActionSet(const Actions& actions)
    : actions_(actions) {}

DeclarativeContentActionSet::~DeclarativeContentActionSet() {}

// static
scoped_ptr<DeclarativeContentActionSet>
DeclarativeContentActionSet::Create(content::BrowserContext* browser_context,
                                    const Extension* extension,
                                    const Values& action_values,
                                    std::string* error,
                                    bool* bad_message) {
  *error = "";
  *bad_message = false;
  Actions result;

  for (const linked_ptr<base::Value>& value : action_values) {
    CHECK(value.get());
    scoped_refptr<const ContentAction> action =
        ContentAction::Create(browser_context, extension, *value, error,
                              bad_message);
    if (!error->empty() || *bad_message)
      return scoped_ptr<DeclarativeContentActionSet>();
    result.push_back(action);
  }

  return make_scoped_ptr(new DeclarativeContentActionSet(result));
}

void DeclarativeContentActionSet::Apply(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    ContentAction::ApplyInfo* apply_info) const {
  for (const scoped_refptr<const ContentAction>& action : actions_)
    action->Apply(extension_id, extension_install_time, apply_info);
}

void DeclarativeContentActionSet::Reapply(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    ContentAction::ApplyInfo* apply_info) const {
  for (const scoped_refptr<const ContentAction>& action : actions_)
    action->Reapply(extension_id, extension_install_time, apply_info);
}

void DeclarativeContentActionSet::Revert(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    ContentAction::ApplyInfo* apply_info) const {
  for (const scoped_refptr<const ContentAction>& action : actions_)
    action->Revert(extension_id, extension_install_time, apply_info);
}

}  // namespace extensions
