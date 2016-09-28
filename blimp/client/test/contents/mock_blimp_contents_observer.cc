// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/test/contents/mock_blimp_contents_observer.h"

namespace blimp {
namespace client {

MockBlimpContentsObserver::MockBlimpContentsObserver(BlimpContents* contents)
    : BlimpContentsObserver(contents) {}

MockBlimpContentsObserver::~MockBlimpContentsObserver() = default;

}  // namespace client
}  // namespace blimp
