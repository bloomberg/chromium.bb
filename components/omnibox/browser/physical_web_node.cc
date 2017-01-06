// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/physical_web_node.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/physical_web/data_source/physical_web_data_source.h"

PhysicalWebNode::PhysicalWebNode(const base::DictionaryValue& metadata_item) {
  std::string title;
  std::string url;
  if (metadata_item.GetString(physical_web::kTitleKey, &title)) {
    node_title_ = base::UTF8ToUTF16(title);
  }
  if (metadata_item.GetString(physical_web::kResolvedUrlKey, &url)) {
    node_url_ = GURL(url);
  }
}

PhysicalWebNode::~PhysicalWebNode() = default;

const base::string16& PhysicalWebNode::GetTitledUrlNodeTitle() const {
  return node_title_;
}

const GURL& PhysicalWebNode::GetTitledUrlNodeUrl() const {
  return node_url_;
}
