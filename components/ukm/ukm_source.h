// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_SOURCE_H_
#define COMPONENTS_UKM_UKM_SOURCE_H_

#include <stddef.h>
#include <map>

#include "base/macros.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace ukm {

class Source;

// Contains UKM data for a single navigation entry.
class UkmSource {
 public:
  UkmSource();
  ~UkmSource();

  int32_t id() const { return id_; }
  void set_id(int32_t id) { id_ = id; }

  const GURL& committed_url() const { return committed_url_; }
  void set_committed_url(const GURL& committed_url) {
    committed_url_ = committed_url;
  }

  base::TimeDelta first_contentful_paint() const {
    return first_contentful_paint_;
  }
  void set_first_contentful_paint(base::TimeDelta first_contentful_paint) {
    first_contentful_paint_ = first_contentful_paint;
  }

  // Serializes the members of the class into the supplied proto.
  void PopulateProto(Source* proto_source);

 private:
  int32_t id_;
  GURL committed_url_;
  base::TimeDelta first_contentful_paint_;

  DISALLOW_COPY_AND_ASSIGN(UkmSource);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_SOURCE_H_
