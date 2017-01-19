// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/physical_web_node.h"

#include "base/strings/utf_string_conversions.h"
#include "components/physical_web/data_source/physical_web_data_source.h"

PhysicalWebNode::PhysicalWebNode(const physical_web::Metadata& metadata_item)
  : node_title_(base::UTF8ToUTF16(metadata_item.title)),
    node_url_(metadata_item.resolved_url) {
}

PhysicalWebNode::~PhysicalWebNode() = default;

const base::string16& PhysicalWebNode::GetTitledUrlNodeTitle() const {
  return node_title_;
}

const GURL& PhysicalWebNode::GetTitledUrlNodeUrl() const {
  return node_url_;
}
