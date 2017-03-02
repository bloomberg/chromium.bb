// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_source.h"

#include "base/hash.h"
#include "components/metrics/proto/ukm/source.pb.h"

namespace ukm {

UkmSource::UkmSource() = default;

UkmSource::~UkmSource() = default;

void UkmSource::PopulateProto(Source* proto_source) const {
  DCHECK(!proto_source->has_id());
  DCHECK(!proto_source->has_url());

  proto_source->set_id(id_);
  proto_source->set_url(url_.spec());
}

}  // namespace ukm
