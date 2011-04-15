// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_COMMON_PARAM_TRAITS_H_
#define CHROME_COMMON_COMMON_PARAM_TRAITS_H_
#pragma once

#include <string>

#include "chrome/common/content_settings.h"
#include "ipc/ipc_param_traits.h"

namespace IPC {

class Message;

template <>
struct ParamTraits<ContentSetting> {
  typedef ContentSetting param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ContentSettingsType> {
  typedef ContentSettingsType param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_COMMON_PARAM_TRAITS_H_
