// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_ROUTING_ID_ISSUER_H_
#define CONTENT_BROWSER_RENDERER_HOST_ROUTING_ID_ISSUER_H_

#include "base/atomic_sequence_num.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message.h"

namespace content {

// A debug facility for tightening the routing ID usage across multiple
// RenderProcessHost. The tracked ID is used by RenderProcessHost
// to verify if the given ID is correct.
class CONTENT_EXPORT RoutingIDIssuer {
 public:
  static scoped_ptr<RoutingIDIssuer> Create();
  static scoped_ptr<RoutingIDIssuer> CreateWithMangler(int mangler);

  int IssueNext();
  bool IsProbablyValid(int issued_id) const;

 private:
  friend class RoutingIDManglingDisabler;

  static void DisableIDMangling();
  static void EnableIDMangling();

  explicit RoutingIDIssuer(int mangler);

  base::AtomicSequenceNumber next_;
  const int mangler_;

  DISALLOW_COPY_AND_ASSIGN(RoutingIDIssuer);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_ROUTING_ID_ISSUER_H_
