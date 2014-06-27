// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_UTILS_H_

#include <cstddef>
#include <string>

#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/values.h"

namespace chromeos {

typedef std::vector<std::string> StringList;
typedef std::vector<base::string16> String16List;

template<typename T>
struct UnwrapConstRef {
  typedef T Type;
};

template<typename T>
struct UnwrapConstRef<const T&> {
  typedef T Type;
};

bool ParseValue(const base::Value* value, bool* out_value);
bool ParseValue(const base::Value* value, int* out_value);
bool ParseValue(const base::Value* value, double* out_value);
bool ParseValue(const base::Value* value, std::string* out_value);
bool ParseValue(const base::Value* value, base::string16* out_value);
bool ParseValue(const base::Value* value,
                const base::DictionaryValue** out_value);
bool ParseValue(const base::Value* value, StringList* out_value);
bool ParseValue(const base::Value* value, String16List* out_value);

template<typename T>
inline bool GetArg(const base::ListValue* args, size_t index, T* out_value) {
  const base::Value* value;
  if (!args->Get(index, &value))
    return false;
  return ParseValue(value, out_value);
}

base::FundamentalValue MakeValue(bool v);
base::FundamentalValue MakeValue(int v);
base::FundamentalValue MakeValue(double v);
base::StringValue MakeValue(const std::string& v);
base::StringValue MakeValue(const base::string16& v);

template<typename T>
inline const T& MakeValue(const T& v) {
  return v;
}

void CallbackWrapper0(base::Callback<void()> callback,
                      const base::ListValue* args);

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
