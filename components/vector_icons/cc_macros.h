// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VECTOR_ICONS_CC_MACROS_H_
#define COMPONENTS_VECTOR_ICONS_CC_MACROS_H_

// This file holds macros that are common to each vector icon target's
// vector_icons.cc.template file.

// The prefix is used to help make sure the string IDs are unique. Typically,
// matching the namespace of the icons should ensure that is the case. If the
// vector_icons.cc.template file doesn't define a prefix, we'll go without one.
#ifndef VECTOR_ICON_ID_PREFIX
#define VECTOR_ICON_ID_PREFIX ""
#endif

#define VECTOR_ICON_REP_TEMPLATE(path_name, rep_name, ...)       \
  static constexpr gfx::PathElement path_name[] = {__VA_ARGS__}; \
  constexpr gfx::VectorIconRep rep_name = {path_name, arraysize(path_name)};

// The VectorIcon will be called kMyIcon, and the identifier for the icon might
// be "my_namespace::kMyIconId".
#define VECTOR_ICON_TEMPLATE(icon_name, rep_name)                \
  const char icon_name##Id[] = VECTOR_ICON_ID_PREFIX #icon_name; \
  const gfx::VectorIcon icon_name = {&rep_name, nullptr, icon_name##Id};

#define VECTOR_ICON_TEMPLATE2(icon_name, rep_name, rep_name_1x)  \
  const char icon_name##Id[] = VECTOR_ICON_ID_PREFIX #icon_name; \
  const gfx::VectorIcon icon_name = {&rep_name, &rep_name_1x, icon_name##Id};

#else  // !COMPONENTS_VECTOR_ICONS_CC_MACROS_H_
#error This file should only be included once.
#endif  // COMPONENTS_VECTOR_ICONS_CC_MACROS_H_
