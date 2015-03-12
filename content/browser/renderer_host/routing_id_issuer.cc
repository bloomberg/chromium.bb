// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/routing_id_issuer.h"

namespace content {

namespace {

const int kManglerBits = 6;
const int kIDBits = 31 - kManglerBits;
const int kManglerWrap = 1 << kManglerBits;
const int kIDWrap = 1 << kIDBits;
const int kIDMask = kIDWrap - 1;

static base::StaticAtomicSequenceNumber g_manglerSequence;
static int g_disablingIDManglingCount = 0;

int GetNextMangler() {
  // Does +1 to always gives some mangler.
  return (g_manglerSequence.GetNext() % (kManglerWrap - 1)) + 1;
}

}  // namespace

scoped_ptr<RoutingIDIssuer> RoutingIDIssuer::Create() {
  if (0 < g_disablingIDManglingCount)
    return make_scoped_ptr(new RoutingIDIssuer(0));

  DCHECK(0 == g_disablingIDManglingCount);
  return make_scoped_ptr(new RoutingIDIssuer(GetNextMangler()));
}

scoped_ptr<RoutingIDIssuer> RoutingIDIssuer::CreateWithMangler(int mangler) {
  return make_scoped_ptr(new RoutingIDIssuer(mangler));
}

RoutingIDIssuer::RoutingIDIssuer(int mangler) : mangler_(mangler) {
  DCHECK(0 <= mangler_ && mangler_ < kManglerWrap) << mangler_
                                                   << ">=" << kManglerWrap;
}

int RoutingIDIssuer::IssueNext() {
  return (mangler_ << kIDBits) | ((next_.GetNext() + 1) & kIDMask);
}

bool RoutingIDIssuer::IsProbablyValid(int issued_id) const {
  return (mangler_ << kIDBits) == (issued_id & ~kIDMask);
}

void RoutingIDIssuer::DisableIDMangling() {
  g_disablingIDManglingCount++;
}

void RoutingIDIssuer::EnableIDMangling() {
  g_disablingIDManglingCount--;
  DCHECK(0 <= g_disablingIDManglingCount);
}

}  // namespace content
