// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_UTILS_H_

#include <cstddef>
#include <string>

#include "base/callback.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/values.h"

namespace chromeos {

template<typename T>
struct UnwrapConstRef {
  typedef T Type;
};

template<typename T>
struct UnwrapConstRef<const T&> {
  typedef T Type;
};

template<typename T>
inline bool GetArg(const base::ListValue* args, size_t index, T* out_value);

template<>
inline bool GetArg<bool>(const base::ListValue* args,
                         size_t index,
                         bool* out_value) {
  return args->GetBoolean(index, out_value);
}

template<>
inline bool GetArg<int>(const base::ListValue* args,
                        size_t index,
                        int* out_value) {
  return args->GetInteger(index, out_value);
}

template<>
inline bool GetArg<double>(const base::ListValue* args,
                           size_t index,
                           double* out_value) {
  return args->GetDouble(index, out_value);
}

template<>
inline bool GetArg<std::string>(const base::ListValue* args,
                                size_t index,
                                std::string* out_value) {
  return args->GetString(index, out_value);
}

template<>
inline bool GetArg<string16>(const base::ListValue* args,
                             size_t index,
                             string16* out_value) {
  return args->GetString(index, out_value);
}

inline void CallbackWrapper0(base::Callback<void()> callback,
                             const base::ListValue* args) {
  DCHECK(args);
  DCHECK(args->empty());
  callback.Run();
}

template<typename A1>
void CallbackWrapper1(base::Callback<void(A1)> callback,
                      const base::ListValue* args) {
  DCHECK(args);
  DCHECK_EQ(1u, args->GetSize());
  typename UnwrapConstRef<A1>::Type arg1;
  if (!GetArg(args, 0, &arg1)) {
    NOTREACHED();
    return;
  }
  callback.Run(arg1);
}

template<typename A1, typename A2>
void CallbackWrapper2(base::Callback<void(A1, A2)> callback,
                      const base::ListValue* args) {
  DCHECK(args);
  DCHECK_EQ(2u, args->GetSize());
  typename UnwrapConstRef<A1>::Type arg1;
  typename UnwrapConstRef<A2>::Type arg2;
  if (!GetArg(args, 0, &arg1) || !GetArg(args, 1, &arg2)) {
    NOTREACHED();
    return;
  }
  callback.Run(arg1, arg2);
}

template<typename A1, typename A2, typename A3>
void CallbackWrapper3(base::Callback<void(A1, A2, A3)> callback,
                      const base::ListValue* args) {
  DCHECK(args);
  DCHECK_EQ(3u, args->GetSize());
  typename UnwrapConstRef<A1>::Type arg1;
  typename UnwrapConstRef<A2>::Type arg2;
  typename UnwrapConstRef<A3>::Type arg3;
  if (!GetArg(args, 0, &arg1) ||
      !GetArg(args, 1, &arg2) ||
      !GetArg(args, 2, &arg3)) {
    NOTREACHED();
    return;
  }
  callback.Run(arg1, arg2, arg3);
}

template<typename A1, typename A2, typename A3, typename A4>
void CallbackWrapper4(base::Callback<void(A1, A2, A3, A4)> callback,
                      const base::ListValue* args) {
  DCHECK(args);
  DCHECK_EQ(4u, args->GetSize());
  typename UnwrapConstRef<A1>::Type arg1;
  typename UnwrapConstRef<A2>::Type arg2;
  typename UnwrapConstRef<A3>::Type arg3;
  typename UnwrapConstRef<A4>::Type arg4;
  if (!GetArg(args, 0, &arg1) ||
      !GetArg(args, 1, &arg2) ||
      !GetArg(args, 2, &arg3) ||
      !GetArg(args, 3, &arg4)) {
    NOTREACHED();
    return;
  }
  callback.Run(arg1, arg2, arg3, arg4);
}

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_UTILS_H_
