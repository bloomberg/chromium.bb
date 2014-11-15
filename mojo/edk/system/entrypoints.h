// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_ENTRYPOINTS_H_
#define MOJO_EDK_SYSTEM_ENTRYPOINTS_H_

namespace mojo {
namespace system {

class Core;

namespace entrypoints {

// Sets the instance of Core to be used by system functions.
void SetCore(Core* core);
// Gets the instance of Core to be used by system functions.
Core* GetCore();

}  // namespace entrypoints
}  // namepace system
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_ENTRYPOINTS_H_
