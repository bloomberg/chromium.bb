// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_PLATFORM_HANDLE_DISPATCHER_H_
#define MOJO_EDK_SYSTEM_PLATFORM_HANDLE_DISPATCHER_H_

#include <stddef.h>

#include <utility>

#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/edk/system/simple_dispatcher.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace edk {

// A dispatcher that simply wraps/transports a |PlatformHandle| (only for use by
// the embedder).
class MOJO_SYSTEM_IMPL_EXPORT PlatformHandleDispatcher final
    : public SimpleDispatcher {
 public:
  static scoped_refptr<PlatformHandleDispatcher> Create(
      ScopedPlatformHandle platform_handle) {
    return make_scoped_refptr(
        new PlatformHandleDispatcher(std::move(platform_handle)));
  }

  ScopedPlatformHandle PassPlatformHandle();

  // |Dispatcher| public methods:
  Type GetType() const override;

  // The "opposite" of |SerializeAndClose()|. (Typically this is called by
  // |Dispatcher::Deserialize()|.)
  static scoped_refptr<PlatformHandleDispatcher> Deserialize(
      const void* source,
      size_t size,
      PlatformHandleVector* platform_handles);

 private:
  explicit PlatformHandleDispatcher(
      ScopedPlatformHandle platform_handle);
  ~PlatformHandleDispatcher() override;

  // |Dispatcher| protected methods:
  void CloseImplNoLock() override;
  scoped_refptr<Dispatcher> CreateEquivalentDispatcherAndCloseImplNoLock()
      override;
  void StartSerializeImplNoLock(size_t* max_size,
                                size_t* max_platform_handles) override;
  bool EndSerializeAndCloseImplNoLock(
      void* destination,
      size_t* actual_size,
      PlatformHandleVector* platform_handles) override;

  ScopedPlatformHandle platform_handle_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(PlatformHandleDispatcher);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_PLATFORM_HANDLE_DISPATCHER_H_
