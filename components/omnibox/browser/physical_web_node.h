// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_PHYSICAL_WEB_NODE_H_
#define COMPONENTS_OMNIBOX_BROWSER_PHYSICAL_WEB_NODE_H_

#include "components/bookmarks/browser/titled_url_node.h"
#include "url/gurl.h"

namespace physical_web {
struct Metadata;
}

class PhysicalWebNode : public bookmarks::TitledUrlNode {
 public:
  explicit PhysicalWebNode(const physical_web::Metadata& metadata_item);
  ~PhysicalWebNode() override;

  // TitledUrlNode
  const base::string16& GetTitledUrlNodeTitle() const override;
  const GURL& GetTitledUrlNodeUrl() const override;

 private:
  base::string16 node_title_;
  GURL node_url_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_PHYSICAL_WEB_NODE_H_
