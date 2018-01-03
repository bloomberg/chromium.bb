// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_USER_DATA_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_USER_DATA_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/previews/core/previews_experiments.h"

namespace base {
class Value;
}

namespace net {
class URLRequest;
}

namespace previews {

// A representation of previews information related to a navigation.
class PreviewsUserData : public base::SupportsUserData::Data {
 public:
  PreviewsUserData(uint64_t page_id);
  ~PreviewsUserData() override;

  // Convert from/to a base::Value.
  base::Value ToValue();
  explicit PreviewsUserData(const base::Value& value);

  // Makes a deep copy.
  std::unique_ptr<PreviewsUserData> DeepCopy() const;

  // Creates the data on |request| if it does not exist, and returns a pointer
  // to the data.
  static PreviewsUserData* Create(net::URLRequest* request, uint64_t page_id);

  // Gets the data from the request if it exists.
  static PreviewsUserData* GetData(const net::URLRequest& request);

  // A session unique ID related to this navigation.
  uint64_t page_id() const { return page_id_; }

  // Sets the committed previews type. Should only be called once.
  void SetCommittedPreviewsType(previews::PreviewsType previews_type);

  // The committed previews type, if any. Otherwise PreviewsType::NONE.
  previews::PreviewsType GetCommittedPreviewsType() const {
    return committed_previews_type_;
  }

  // Whether there is a committed previews type.
  bool HasCommittedPreviewsType() const {
    return committed_previews_type_ != previews::PreviewsType::NONE;
  }

 private:
  // A session unique ID related to this navigation.
  const uint64_t page_id_;

  // The committed previews type, if any.
  previews::PreviewsType committed_previews_type_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsUserData);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_USER_DATA_H_
