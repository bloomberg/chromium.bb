// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_ROUTIND_ID_MANGLING_DISABLER_H_
#define CONTENT_PUBLIC_TEST_ROUTIND_ID_MANGLING_DISABLER_H_

namespace content {

// Some tests unfortunately assume that the routing ID starts from
// one. This class temporarily satisfies such an assumption.
class RoutingIDManglingDisabler {
 public:
  RoutingIDManglingDisabler();
  ~RoutingIDManglingDisabler();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_ROUTIND_ID_MANGLING_DISABLER_H_
