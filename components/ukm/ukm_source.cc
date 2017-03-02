// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_source.h"

#include "base/hash.h"
#include "components/metrics/proto/ukm/source.pb.h"

namespace ukm {

UkmSource::UkmSource() = default;

UkmSource::~UkmSource() = default;

void UkmSource::UpdateUrl(const GURL& url) {
  DCHECK(!url_.is_empty());
  if (url_ == url)
    return;
  if (initial_url_.is_empty())
    initial_url_ = url_;
  url_ = url;
}

void UkmSource::PopulateProto(Source* proto_source) const {
  DCHECK(!proto_source->has_id());
  DCHECK(!proto_source->has_url());
  DCHECK(!proto_source->has_initial_url());

  proto_source->set_id(id_);
  proto_source->set_url(url_.spec());
  if (!initial_url_.is_empty())
    proto_source->set_initial_url(initial_url_.spec());
}

}  // namespace ukm
