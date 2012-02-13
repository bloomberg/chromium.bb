// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_KEYED_BASE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_KEYED_BASE_H_

// A root class used by the ProfileKeyedBaseFactory.
//
// ProfileKeyedBaseFactory is a generalization of dependency and lifetime
// management, leaving the actual implementation of storage and what actually
// happens at lifetime events up to the factory type.
class ProfileKeyedBase {
 public:
  virtual ~ProfileKeyedBase() {}
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_KEYED_BASE_H_
