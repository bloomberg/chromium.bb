// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_RENDER_MESSAGES_PARAMS_H_
#define CHROME_COMMON_RENDER_MESSAGES_PARAMS_H_
#pragma once

#include <string>

#include "ipc/ipc_param_traits.h"

// The type of OSDD that the renderer is giving to the browser.
struct ViewHostMsg_PageHasOSDD_Type {
  enum Type {
    // The Open Search Description URL was detected automatically.
    AUTODETECTED_PROVIDER,

    // The Open Search Description URL was given by Javascript.
    EXPLICIT_PROVIDER,

    // The Open Search Description URL was given by Javascript to be the new
    // default search engine.
    EXPLICIT_DEFAULT_PROVIDER
  };

  Type type;

  ViewHostMsg_PageHasOSDD_Type() : type(AUTODETECTED_PROVIDER) {
  }

  explicit ViewHostMsg_PageHasOSDD_Type(Type t)
      : type(t) {
  }

  static ViewHostMsg_PageHasOSDD_Type Autodetected() {
    return ViewHostMsg_PageHasOSDD_Type(AUTODETECTED_PROVIDER);
  }

  static ViewHostMsg_PageHasOSDD_Type Explicit() {
    return ViewHostMsg_PageHasOSDD_Type(EXPLICIT_PROVIDER);
  }

  static ViewHostMsg_PageHasOSDD_Type ExplicitDefault() {
    return ViewHostMsg_PageHasOSDD_Type(EXPLICIT_DEFAULT_PROVIDER);
  }
};

// The install state of the search provider (not installed, installed, default).
struct ViewHostMsg_GetSearchProviderInstallState_Params {
  enum State {
    // Equates to an access denied error.
    DENIED = -1,

    // DON'T CHANGE THE VALUES BELOW.
    // All of the following values are manidated by the
    // spec for window.external.IsSearchProviderInstalled.

    // The search provider is not installed.
    NOT_INSTALLED = 0,

    // The search provider is in the user's set but is not
    INSTALLED_BUT_NOT_DEFAULT = 1,

    // The search provider is set as the user's default.
    INSTALLED_AS_DEFAULT = 2
  };
  State state;

  ViewHostMsg_GetSearchProviderInstallState_Params()
      : state(DENIED) {
  }

  explicit ViewHostMsg_GetSearchProviderInstallState_Params(State s)
      : state(s) {
  }

  static ViewHostMsg_GetSearchProviderInstallState_Params Denied() {
    return ViewHostMsg_GetSearchProviderInstallState_Params(DENIED);
  }

  static ViewHostMsg_GetSearchProviderInstallState_Params NotInstalled() {
    return ViewHostMsg_GetSearchProviderInstallState_Params(NOT_INSTALLED);
  }

  static ViewHostMsg_GetSearchProviderInstallState_Params
      InstallButNotDefault() {
    return ViewHostMsg_GetSearchProviderInstallState_Params(
        INSTALLED_BUT_NOT_DEFAULT);
  }

  static ViewHostMsg_GetSearchProviderInstallState_Params InstalledAsDefault() {
    return ViewHostMsg_GetSearchProviderInstallState_Params(
        INSTALLED_AS_DEFAULT);
  }
};

namespace IPC {

class Message;

template <>
struct ParamTraits<ViewHostMsg_PageHasOSDD_Type> {
  typedef ViewHostMsg_PageHasOSDD_Type param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ViewHostMsg_GetSearchProviderInstallState_Params> {
  typedef ViewHostMsg_GetSearchProviderInstallState_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_RENDER_MESSAGES_PARAMS_H_
