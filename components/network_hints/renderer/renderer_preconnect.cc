// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See header file for description of RendererPreconnect class

#include "components/network_hints/renderer/renderer_preconnect.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "components/network_hints/common/network_hints_common.h"
#include "components/network_hints/common/network_hints_messages.h"
#include "content/public/renderer/render_thread.h"

using content::RenderThread;

namespace network_hints {

RendererPreconnect::RendererPreconnect()
    : weak_factory_(this) {
}

RendererPreconnect::~RendererPreconnect() {
}

// Push URLs into the map quickly!
void RendererPreconnect::Preconnect(const GURL &url) {
  if (!url.is_valid())
    return;

  // If this is the first entry in the map then a task needs to be scheduled.
  bool needs_task = url_request_count_map_.empty();

  // ints will initialize to 0 automatically so incrementing will initialize
  // new URLs to 1 or increment existing URLs (exactly the desired behavior).
  url_request_count_map_[url]++;

  if (needs_task) {
    weak_factory_.InvalidateWeakPtrs();
    RenderThread::Get()->GetTaskRunner()->PostDelayedTask(
        FROM_HERE, base::Bind(&RendererPreconnect::SubmitPreconnect,
                              weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(10));
  }
}

// Extract data from the Map, and then send it off the the Browser process
// to be preconnected.
void RendererPreconnect::SubmitPreconnect() {
  DCHECK(!url_request_count_map_.empty());
  for (const auto& entry : url_request_count_map_) {
    RenderThread::Get()->Send(
        new NetworkHintsMsg_Preconnect(entry.first, entry.second));
  }
  url_request_count_map_.clear();
}

}  // namespace network_hints
