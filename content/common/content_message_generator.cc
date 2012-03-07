// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Get basic type definitions.
#define IPC_MESSAGE_IMPL
#include "content/common/content_message_generator.h"

// Generate constructors.
#include "ipc/struct_constructor_macros.h"
#include "content/common/content_message_generator.h"

// Generate destructors.
#include "ipc/struct_destructor_macros.h"
#include "content/common/content_message_generator.h"

#if defined(USE_AURA) && defined(OS_WIN)
#include "ui/gfx/native_widget_types.h"

namespace IPC {
// TODO(beng): Figure out why this is needed, fix that issue and remove
//             this. Brett and I were unable to figure out why, but he
//             thought this should be harmless.
template <>
struct ParamTraits<gfx::PluginWindowHandle> {
  typedef gfx::PluginWindowHandle param_type;
  static void Write(Message* m, const param_type& p) {
    m->WriteUInt32(reinterpret_cast<uint32>(p));
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    DCHECK_EQ(sizeof(param_type), sizeof(uint32));
    return m->ReadUInt32(iter, reinterpret_cast<uint32*>(r));
  }
  static void Log(const param_type& p, std::string* l) {
    l->append(StringPrintf("0x%X", p));
  }
};
}  // namespace IPC
#endif

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "content/common/content_message_generator.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "content/common/content_message_generator.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "content/common/content_message_generator.h"
}  // namespace IPC
