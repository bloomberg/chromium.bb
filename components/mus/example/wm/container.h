// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_EXAMPLE_WM_IDS_H_
#define COMPONENTS_MUS_EXAMPLE_WM_IDS_H_

enum class Container {
  ALL_USER_BACKGROUND = 1,
  USER_WORKSPACE,
  USER_BACKGROUND,
  USER_PRIVATE,
  USER_WINDOWS,
  USER_STICKY_WINDOWS,
  USER_PRESENTATION_WINDOWS,
  USER_LAUNCHER,
  LOGIN_WINDOWS,
  LOGIN_APP,    // TODO(beng): what about dialog boxes login opens?
  LOGIN_LAUNCHER,
  SYSTEM_MODAL_WINDOWS,
  KEYBOARD,
  MENUS,
  TOOLTIPS,
  COUNT
};

#endif  // COMPONENTS_MUS_EXAMPLE_WM_IDS_H_
