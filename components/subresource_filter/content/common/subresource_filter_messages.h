// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.
// no-include-guard-because-multiply-included

#include "base/time/time.h"
#include "components/subresource_filter/mojom/subresource_filter.mojom.h"
#include "content/public/common/common_param_traits_macros.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "url/ipc/url_param_traits.h"

#define IPC_MESSAGE_START SubresourceFilterMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(subresource_filter::mojom::ActivationLevel,
                          subresource_filter::mojom::ActivationLevel::kMaxValue)

IPC_STRUCT_TRAITS_BEGIN(subresource_filter::mojom::ActivationState)
  IPC_STRUCT_TRAITS_MEMBER(activation_level)
  IPC_STRUCT_TRAITS_MEMBER(filtering_disabled_for_document)
  IPC_STRUCT_TRAITS_MEMBER(generic_blocking_rules_disabled)
  IPC_STRUCT_TRAITS_MEMBER(measure_performance)
  IPC_STRUCT_TRAITS_MEMBER(enable_logging)
IPC_STRUCT_TRAITS_END()

// ----------------------------------------------------------------------------
// Messages sent from the browser to the renderer.
// ----------------------------------------------------------------------------

// Sends a read-only mode file handle with the ruleset data to a renderer
// process, containing the subresource filtering rules to be consulted for all
// subsequent document loads that have subresource filtering activated.
IPC_MESSAGE_CONTROL1(SubresourceFilterMsg_SetRulesetForProcess,
                     IPC::PlatformFileForTransit /* ruleset_file */)

// Instructs the renderer to activate subresource filtering at the specified
// |activation_state| for the document load committed next in a frame.
//
// Without browser-side navigation, the message must arrive just before the
// provisional load is committed on the renderer side. In practice, it is often
// sent after the provisional load has already started, but this is not a
// requirement. The message will have no effect if the provisional load fails.
//
// With browser-side navigation enabled, the message must arrive just before
// mojom::FrameNavigationControl::CommitNavigation.
//
// If no message arrives, the default behavior is
// mojom::ActivationLevel::kDisabled.
IPC_MESSAGE_ROUTED2(
    SubresourceFilterMsg_ActivateForNextCommittedLoad,
    subresource_filter::mojom::ActivationState /* activation_state */,
    bool /* is_ad_subframe */)
