// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/event_matcher.h"

namespace mus {

mojo::EventMatcherPtr CreateKeyMatcher(mojo::KeyboardCode code,
                                       mojo::EventFlags flags) {
  mojo::EventMatcherPtr matcher(mojo::EventMatcher::New());
  matcher->type_matcher = mojo::EventTypeMatcher::New();
  matcher->flags_matcher = mojo::EventFlagsMatcher::New();
  matcher->key_matcher = mojo::KeyEventMatcher::New();

  matcher->type_matcher->type = mojo::EVENT_TYPE_KEY_PRESSED;
  matcher->flags_matcher->flags = flags;
  matcher->key_matcher->keyboard_code = code;
  return matcher.Pass();
}

}  // namespace mus
