// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_SOURCE_H_
#define COMPONENTS_UKM_UKM_SOURCE_H_

#include <map>

#include "base/macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace ukm {

class Source;

// Contains UKM data for a single navigation entry.
class UkmSource {
 public:
  enum CustomTabState {
    kCustomTabUnset,
    kCustomTabTrue,
    kCustomTabFalse,
  };

  UkmSource();
  ~UkmSource();

  ukm::SourceId id() const { return id_; }
  void set_id(ukm::SourceId id) { id_ = id; }

  const GURL& initial_url() const { return initial_url_; }
  const GURL& url() const { return url_; }

  // The object creation time. This is for internal purposes only and is not
  // intended to be anything useful for UKM clients.
  const base::TimeTicks creation_time() const { return creation_time_; }

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

  // Sets the current "custom tab" state. This can be called from any thread.
  static void SetCustomTabVisible(bool visible);

 private:
  ukm::SourceId id_;

  // The final, canonical URL for this source.
  GURL url_;

  // The initial URL for this source. Only set if different from |url_| (i.e. if
  // the URL changed over the lifetime of this source).
  GURL initial_url_;

  // A flag indicating if metric was collected in a custom tab. This is set
  // automatically when the object is created and so represents the state when
  // the metric was created.
  const CustomTabState custom_tab_state_;

  // When this object was created.
  const base::TimeTicks creation_time_;

  DISALLOW_COPY_AND_ASSIGN(UkmSource);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_SOURCE_H_
