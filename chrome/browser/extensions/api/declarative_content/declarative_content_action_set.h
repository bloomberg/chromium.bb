// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_ACTION_SET_H__
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_ACTION_SET_H__

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "chrome/browser/extensions/api/declarative_content/content_action.h"

namespace base {
class Time;
class Value;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;

// Immutable container for multiple actions.
//
// TODO(battre): As DeclarativeContentActionSet can become the single owner of
// all actions, we can optimize here by making some of them singletons
// (e.g. Cancel actions).
class DeclarativeContentActionSet {
 public:
  using Values = std::vector<linked_ptr<base::Value>>;
  using Actions = std::vector<scoped_refptr<const ContentAction>>;

  explicit DeclarativeContentActionSet(const Actions& actions);
  ~DeclarativeContentActionSet();

  // Factory method that instantiates a DeclarativeContentActionSet for
  // |extension| according to |actions| which represents the array of actions
  // received from the extension API.
  static scoped_ptr<DeclarativeContentActionSet> Create(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const Values& action_values,
      std::string* error,
      bool* bad_message);

  // Rules call this method when their conditions are fulfilled.
  void Apply(const std::string& extension_id,
             const base::Time& extension_install_time,
             ContentAction::ApplyInfo* apply_info) const;

  // Rules call this method when their conditions are fulfilled, but Apply has
  // already been called.
  void Reapply(const std::string& extension_id,
               const base::Time& extension_install_time,
               ContentAction::ApplyInfo* apply_info) const;

  // Rules call this method when they have stateful conditions, and those
  // conditions stop being fulfilled.  Rules with event-based conditions (e.g. a
  // network request happened) will never Revert() an action.
  void Revert(const std::string& extension_id,
              const base::Time& extension_install_time,
              ContentAction::ApplyInfo* apply_info) const;

  const Actions& actions() const { return actions_; }

 private:
  const Actions actions_;

  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentActionSet);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_ACTION_SET_H__
