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

  const GURL& initial_url() const { return initial_url_; }
  const GURL& url() const { return url_; }

  // Sets the URL for this source. Should be invoked when a source is
  // initialized.
  void set_url(const GURL& url) { url_ = url; }

  // Updates the URL for this source. Must be called after set_url. If a new URL
  // is passed to UpdateUrl, the initial_url field is populated with the
  // original URL provided to set_url, and the url field is updated with the
  // value provided to this method.
  void UpdateUrl(const GURL& url);

  // Serializes the members of the class into the supplied proto.
  void PopulateProto(Source* proto_source) const;

 private:
  int32_t id_;

  // The final, canonical URL for this source.
  GURL url_;

  // The initial URL for this source. Only set if different from |url_| (i.e. if
  // the URL changed over the lifetime of this source).
  GURL initial_url_;

  DISALLOW_COPY_AND_ASSIGN(UkmSource);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_SOURCE_H_
