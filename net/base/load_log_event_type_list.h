// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE: No header guards are used, since this file is intended to be expanded
// directly into load_log.h. DO NOT include this file anywhere else.

// --------------------------------------------------------------------------
// General pseudo-events
// --------------------------------------------------------------------------

// If a log is terminated by a kLogTruncated event, then it grew too large
// and any subsequent events logged to it were discarded.
EVENT_TYPE(LOG_TRUNCATED)

// Something got cancelled (we determine what is cancelled based on the
// log context around it.)
EVENT_TYPE(CANCELLED)

// ------------------------------------------------------------------------
// HostResolverImpl
// ------------------------------------------------------------------------

// The start/end of a host resolve (DNS) request.
EVENT_TYPE(HOST_RESOLVER_IMPL)

// The start/end of HostResolver::Observer::OnStartResolution.
EVENT_TYPE(HOST_RESOLVER_IMPL_OBSERVER_ONSTART)

// The start/end of HostResolver::Observer::OnFinishResolutionWithStatus
EVENT_TYPE(HOST_RESOLVER_IMPL_OBSERVER_ONFINISH)

// The start/end of HostResolver::Observer::OnCancelResolution.
EVENT_TYPE(HOST_RESOLVER_IMPL_OBSERVER_ONCANCEL)
