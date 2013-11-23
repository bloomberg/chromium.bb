// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/converter.h"
#include "gin/public/gin_embedders.h"
#include "gin/public/wrapper_info.h"
#include "gin/wrappable.h"

#include "v8/include/v8.h"

namespace gin {

class PerIsolateData;

template<typename T>
struct RemoveConstRef {
  typedef T Type;
};
template<typename T>
struct RemoveConstRef<const T&> {
  typedef T Type;
};

class CallbackHolderBase : public Wrappable {
 public:
  static void EnsureRegistered(PerIsolateData* isolate_data);
  virtual WrapperInfo* GetWrapperInfo() OVERRIDE;
  static WrapperInfo kWrapperInfo;
 protected:
  virtual ~CallbackHolderBase() {}
};

template<>
struct Converter<CallbackHolderBase*>
    : public WrappableConverter<CallbackHolderBase> {};

template<typename Sig>
class CallbackHolder : public CallbackHolderBase {
 public:
  CallbackHolder(const base::Callback<Sig>& callback)
      : callback(callback) {}
  base::Callback<Sig> callback;
 private:
  virtual ~CallbackHolder() {}
};

// TODO(aa): Generate the overloads below with pump.

template<typename P1, typename P2>
static void DispatchToCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  Arguments args(info);
  CallbackHolderBase* holder_base = NULL;
  CHECK(args.GetData(&holder_base));

  typedef CallbackHolder<void(P1, P2)> HolderT;
  HolderT* holder = static_cast<HolderT*>(holder_base);

  typename RemoveConstRef<P1>::Type a1;
  typename RemoveConstRef<P2>::Type a2;
  if (!args.GetNext(&a1) ||
      !args.GetNext(&a2)) {
    args.ThrowError();
    return;
  }

  holder->callback.Run(a1, a2);
}

template<typename P1, typename P2>
v8::Local<v8::FunctionTemplate> CreateFunctionTempate(
    v8::Isolate* isolate,
    const base::Callback<void(P1, P2)>& callback) {
  typedef CallbackHolder<void(P1, P2)> HolderT;
  scoped_refptr<HolderT> holder(new HolderT(callback));
  return v8::FunctionTemplate::New(
      &DispatchToCallback<P1, P2>,
      ConvertToV8<CallbackHolderBase*>(isolate, holder.get()));
}

}  // namespace gin
