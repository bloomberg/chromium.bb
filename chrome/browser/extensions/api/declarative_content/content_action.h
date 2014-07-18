// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_ACTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_ACTION_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/declarative/declarative_rule.h"

class Profile;

namespace base {
class Time;
class Value;
}

namespace content {
class WebContents;
}

namespace extensions {
class Extension;

// Base class for all ContentActions of the declarative content API.
class ContentAction : public base::RefCounted<ContentAction> {
 public:
  // Type identifiers for concrete ContentActions.
  enum Type {
    ACTION_SHOW_PAGE_ACTION,
    ACTION_REQUEST_CONTENT_SCRIPT,
  };

  struct ApplyInfo {
    Profile* profile;
    content::WebContents* tab;
  };

  ContentAction();

  virtual Type GetType() const = 0;

  // Applies or reverts this ContentAction on a particular tab for a particular
  // extension.  Revert exists to keep the actions up to date as the page
  // changes.
  virtual void Apply(const std::string& extension_id,
                     const base::Time& extension_install_time,
                     ApplyInfo* apply_info) const = 0;
  virtual void Revert(const std::string& extension_id,
                      const base::Time& extension_install_time,
                      ApplyInfo* apply_info) const = 0;

  // Factory method that instantiates a concrete ContentAction
  // implementation according to |json_action|, the representation of the
  // ContentAction as received from the extension API.
  // Sets |error| and returns NULL in case of a semantic error that cannot
  // be caught by schema validation. Sets |bad_message| and returns NULL
  // in case the input is syntactically unexpected.
  static scoped_refptr<ContentAction> Create(const Extension* extension,
                                             const base::Value& json_action,
                                             std::string* error,
                                             bool* bad_message);

 protected:
  friend class base::RefCounted<ContentAction>;
  virtual ~ContentAction();
};

typedef DeclarativeActionSet<ContentAction> ContentActionSet;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_ACTION_H_
