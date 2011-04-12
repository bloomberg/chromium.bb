// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditional include guard.
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"

// TODO : Pull ViewHostMsg_UpdateFaviconURL into this file

#ifndef CHROME_COMMON_ICON_MESSAGES_H__
#define CHROME_COMMON_ICON_MESSAGES_H__

// The icon type in a page. The definition must be same as history::IconType.
enum IconType {
  INVALID_ICON = 0x0,
  FAVICON = 1 << 0,
  TOUCH_ICON = 1 << 1,
  TOUCH_PRECOMPOSED_ICON = 1 << 2
};

// The favicon url from the render.
struct FaviconURL {
  FaviconURL();
  FaviconURL(const GURL& url, IconType type);
  ~FaviconURL();

  // The url of the icon.
  GURL icon_url;

  // The type of the icon
  IconType icon_type;
};

namespace IPC {

template <>
struct ParamTraits<IconType> {
  typedef IconType param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<FaviconURL> {
  typedef FaviconURL param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_ICON_MESSAGES_H__
