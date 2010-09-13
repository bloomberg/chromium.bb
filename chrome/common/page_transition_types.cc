// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/page_transition_types.h"

#include "base/logging.h"

// static
PageTransition::Type PageTransition::FromInt(int32 type) {
  if (!ValidType(type)) {
    NOTREACHED() << "Invalid transition type " << type;

    // Return a safe default so we don't have corrupt data in release mode.
    return LINK;
  }
  return static_cast<Type>(type);
}

// static
const char* PageTransition::CoreTransitionString(Type type) {
  switch (type & PageTransition::CORE_MASK) {
    case 0: return "link";
    case 1: return "typed";
    case 2: return "auto_bookmark";
    case 3: return "auto_subframe";
    case 4: return "manual_subframe";
    case 5: return "generated";
    case 6: return "start_page";
    case 7: return "form_submit";
    case 8: return "reload";
    case 9: return "keyword";
    case 10: return "keyword_generated";
  }
  return NULL;
}

// static
const char* PageTransition::QualifierString(Type type) {
  DCHECK_NE((int)(type & (CLIENT_REDIRECT | SERVER_REDIRECT)),
            (int)(CLIENT_REDIRECT | SERVER_REDIRECT));

  switch (type & (CLIENT_REDIRECT | SERVER_REDIRECT | FORWARD_BACK)) {
    case CLIENT_REDIRECT: return "client_redirect";
    case SERVER_REDIRECT: return "server_redirect";
    case FORWARD_BACK:    return "forward_back";
    case (CLIENT_REDIRECT | FORWARD_BACK):
                          return "client_redirect|forward_back";
    case (SERVER_REDIRECT | FORWARD_BACK):
                          return "server_redirect|forward_back";
  }
  return "";
}
