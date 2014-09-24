// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_UTIL_CONTAINER_PRIORITIES_H_
#define ATHENA_UTIL_CONTAINER_PRIORITIES_H_

namespace athena {

enum ContainerPriorities {
  CP_BACKGROUND = 0,
  CP_DEFAULT,
  CP_HOME_CARD,
  CP_SYSTEM_MODAL,
  CP_LOGIN_SCREEN,
  CP_LOGIN_SCREEN_SYSTEM_MODAL,
  CP_VIRTUAL_KEYBOARD,
};

}  // namespace athena

#endif  // ATHENA_UTIL_CONTAINER_PRIORITIES_H_
