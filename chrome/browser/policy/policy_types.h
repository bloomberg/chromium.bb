// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_TYPES_H_
#define CHROME_BROWSER_POLICY_POLICY_TYPES_H_

namespace policy {

// The scope of a policy flags whether it is meant to be applied to the current
// user or to the machine.
enum PolicyScope {
  // USER policies apply to sessions of the current user.
  POLICY_SCOPE_USER,

  // MACHINE policies apply to any users of the current machine.
  POLICY_SCOPE_MACHINE,
};

// The level of a policy determines its enforceability and whether users can
// override it or not. The values are listed in increasing order of priority.
enum PolicyLevel {
  // RECOMMENDED policies can be overridden by users. They are meant as a
  // default value configured by admins, that users can customize.
  POLICY_LEVEL_RECOMMENDED,

  // MANDATORY policies must be enforced and users can't circumvent them.
  POLICY_LEVEL_MANDATORY,
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_TYPES_H_
