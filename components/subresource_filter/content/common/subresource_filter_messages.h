// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.

#include "base/time/time.h"
#include "components/subresource_filter/content/common/document_load_statistics.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "content/public/common/common_param_traits_macros.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_platform_file.h"
#include "url/ipc/url_param_traits.h"

#define IPC_MESSAGE_START SubresourceFilterMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(subresource_filter::ActivationState,
                          subresource_filter::ActivationState::LAST);

IPC_STRUCT_TRAITS_BEGIN(subresource_filter::DocumentLoadStatistics)
  IPC_STRUCT_TRAITS_MEMBER(num_loads_total)
  IPC_STRUCT_TRAITS_MEMBER(num_loads_evaluated)
  IPC_STRUCT_TRAITS_MEMBER(num_loads_matching_rules)
  IPC_STRUCT_TRAITS_MEMBER(num_loads_disallowed)
  IPC_STRUCT_TRAITS_MEMBER(evaluation_total_wall_duration)
  IPC_STRUCT_TRAITS_MEMBER(evaluation_total_cpu_duration)
IPC_STRUCT_TRAITS_END()

// ----------------------------------------------------------------------------
// Messages sent from the browser to the renderer.
// ----------------------------------------------------------------------------

// Sends a read-only mode file handle with the ruleset data to a renderer
// process, containing the subresource filtering rules to be consulted for all
// subsequent document loads that have subresource filtering activated.
IPC_MESSAGE_CONTROL1(SubresourceFilterMsg_SetRulesetForProcess,
                     IPC::PlatformFileForTransit /* ruleset_file */);

// Instructs the renderer to activate subresource filtering for the currently
// ongoing provisional document load in a frame. The message must arrive after
// the provisional load starts, but before it is committed on the renderer side.
// If no message arrives, the default behavior is ActivationState::DISABLED.
IPC_MESSAGE_ROUTED3(SubresourceFilterMsg_ActivateForProvisionalLoad,
                    subresource_filter::ActivationState /* activation_state */,
                    GURL /* url */,
                    bool /* measure_performance */);

// ----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.
// ----------------------------------------------------------------------------

// Sent to the browser the first time a subresource load is disallowed for the
// most recently commited document load in a frame. It is used to trigger a
// UI prompt to inform the user and allow them to turn off filtering.
IPC_MESSAGE_ROUTED0(SubresourceFilterHostMsg_DidDisallowFirstSubresource);

// This is sent to a RenderFrameHost in the browser when a document load is
// finished, just before the DidFinishLoad message, and contains statistics
// collected by the DocumentSubresourceFilter up until that point: the number of
// subresources evaluated/disallowed/etc, and total time spent on evaluating
// subresource loads in its allowLoad method. The time metrics are equal to zero
// if performance measurements were disabled for the load.
IPC_MESSAGE_ROUTED1(
    SubresourceFilterHostMsg_DocumentLoadStatistics,
    subresource_filter::DocumentLoadStatistics /* statistics */);
