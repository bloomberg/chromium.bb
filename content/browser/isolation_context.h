// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ISOLATION_CONTEXT_H_
#define CONTENT_BROWSER_ISOLATION_CONTEXT_H_

#include "base/optional.h"
#include "content/common/content_export.h"
#include "gpu/command_buffer/common/id_type.h"

namespace content {

class BrowsingInstance;
using BrowsingInstanceId = gpu::IdType32<BrowsingInstance>;

// This class is used to specify the context in which process model decisions
// need to be made.  For example, dynamically added isolated origins only take
// effect in future BrowsingInstances, and this class can be used to specify
// that a process model decision is being made from a specific
// BrowsingInstance, so that only isolated origins that are applicable to that
// BrowsingInstance are used. This object may be used on UI or IO threads.
class CONTENT_EXPORT IsolationContext {
 public:
  // A default-constructed IsolationContext is not associated with a specific
  // BrowsingInstance.  Callers can use this when they don't know the current
  // BrowsingInstance, or aren't associated with one.
  //
  // TODO(alexmos):  This is primarily used in tests, as well as in call sites
  // which do not yet plumb proper BrowsingInstance information.  Once the
  // remaining non-test call sites are removed or updated, this should become a
  // test-only API.
  IsolationContext();

  explicit IsolationContext(BrowsingInstanceId browsing_instance_id);
  ~IsolationContext() = default;

  // Returns the BrowsingInstance ID associated with this isolation context.
  // BrowsingInstance IDs are ordered such that BrowsingInstances with lower
  // IDs were created earlier than BrowsingInstances with higher IDs.
  //
  // If this is not specified (i.e., |browsing_instance_id().is_null()| is
  // true), then this IsolationContext isn't restricted to any particular
  // BrowsingInstance.  Asking for isolated origins from an IsolationContext
  // with a null |browsing_instance_id()| will return the latest available
  // isolated origins.
  BrowsingInstanceId browsing_instance_id() const {
    return browsing_instance_id_;
  }

 private:
  // When non-null, associates this context with a particular BrowsingInstance.
  BrowsingInstanceId browsing_instance_id_;

  // TODO(alexmos): Include BrowserContext information here as well.  Replace
  // process model APIs that pass in both BrowserContext and IsolationContext
  // to only pass in IsolationContext.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ISOLATION_CONTEXT_H_
