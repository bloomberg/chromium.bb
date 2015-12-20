// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_COMMON_CREATE_BLIMP_MESSAGE_H_
#define BLIMP_COMMON_CREATE_BLIMP_MESSAGE_H_

#include <stdint.h>

#include <string>

#include "base/memory/scoped_ptr.h"
#include "blimp/common/blimp_common_export.h"

namespace blimp {

class BlimpMessage;
class CompositorMessage;
class TabControlMessage;
class InputMessage;
class NavigationMessage;
class RenderWidgetMessage;
class SizeMessage;
class StartConnectionMessage;

// Suite of helper methods to simplify the repetitive task of creating
// new BlimpMessages, initializing them, and extracting type-specific
// inner messages.
//
//
// Every specialization of CreateBlimpMessage returns an initialized
// BlimpMessage object. In addition, a pointer to the type-specific inner
// message is returned via the initial double-pointer parameter.
//
// Additional initialization arguments may be taken depending on the
// message type.

BLIMP_COMMON_EXPORT scoped_ptr<BlimpMessage> CreateBlimpMessage(
    CompositorMessage** compositor_message,
    int target_tab_id);

BLIMP_COMMON_EXPORT scoped_ptr<BlimpMessage> CreateBlimpMessage(
    TabControlMessage** control_message);

BLIMP_COMMON_EXPORT scoped_ptr<BlimpMessage> CreateBlimpMessage(
    InputMessage** input_message);

BLIMP_COMMON_EXPORT scoped_ptr<BlimpMessage> CreateBlimpMessage(
    NavigationMessage** navigation_message,
    int target_tab_id);

BLIMP_COMMON_EXPORT scoped_ptr<BlimpMessage> CreateBlimpMessage(
    RenderWidgetMessage** render_widget_message,
    int target_tab_id);

BLIMP_COMMON_EXPORT scoped_ptr<BlimpMessage> CreateBlimpMessage(
    SizeMessage** size_message);

BLIMP_COMMON_EXPORT scoped_ptr<BlimpMessage> CreateStartConnectionMessage(
    const std::string& client_token,
    int protocol_version);

BLIMP_COMMON_EXPORT scoped_ptr<BlimpMessage> CreateCheckpointAckMessage(
    int64_t checkpoint_id);

}  // namespace blimp

#endif  // BLIMP_COMMON_CREATE_BLIMP_MESSAGE_H_
