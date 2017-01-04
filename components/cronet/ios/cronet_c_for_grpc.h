// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_IOS_CRONET_C_FOR_GRPC_H_
#define COMPONENTS_CRONET_IOS_CRONET_C_FOR_GRPC_H_

#include "bidirectional_stream_c.h"

// TODO(mef): Remove this header after transition to bidirectional_stream_c.h
// See crbug.com/650462 for details.

/* Define deprecated Cronet Engine API using current API. */
#define cronet_engine stream_engine

/* Define deprecated Bidirectional Stream  API using current API. */
#define cronet_bidirectional_stream bidirectional_stream
#define cronet_bidirectional_stream_header bidirectional_stream_header
#define cronet_bidirectional_stream_header_array \
  bidirectional_stream_header_array
#define cronet_bidirectional_stream_callback bidirectional_stream_callback

#define cronet_bidirectional_stream_create bidirectional_stream_create
#define cronet_bidirectional_stream_destroy bidirectional_stream_destroy
#define cronet_bidirectional_stream_disable_auto_flush \
  bidirectional_stream_disable_auto_flush
#define cronet_bidirectional_stream_delay_request_headers_until_flush \
  bidirectional_stream_delay_request_headers_until_flush
#define cronet_bidirectional_stream_start bidirectional_stream_start
#define cronet_bidirectional_stream_read bidirectional_stream_read
#define cronet_bidirectional_stream_write bidirectional_stream_write
#define cronet_bidirectional_stream_flush bidirectional_stream_flush
#define cronet_bidirectional_stream_cancel bidirectional_stream_cancel
#define cronet_bidirectional_stream_is_done bidirectional_stream_is_done

#endif  // COMPONENTS_CRONET_IOS_CRONET_C_FOR_GRPC_H_
