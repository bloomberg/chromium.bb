// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/flat_dom_tree_extractor.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "headless/public/headless_devtools_client.h"

namespace headless {

FlatDomTreeExtractor::FlatDomTreeExtractor(
    HeadlessDevToolsClient* devtools_client)
    : work_in_progress_(false),
      devtools_client_(devtools_client),
      weak_factory_(this) {}

FlatDomTreeExtractor::~FlatDomTreeExtractor() {}

void FlatDomTreeExtractor::ExtractDomTree(
    const std::vector<std::string>& css_style_whitelist,
    DomResultCB callback) {
  DCHECK(!work_in_progress_);
  work_in_progress_ = true;

  callback_ = std::move(callback);

  devtools_client_->GetDOM()->Enable();
  devtools_client_->GetDOM()->GetFlattenedDocument(
      dom::GetFlattenedDocumentParams::Builder()
          .SetDepth(-1)
          .SetPierce(true)
          .Build(),
      base::Bind(&FlatDomTreeExtractor::OnDocumentFetched,
                 weak_factory_.GetWeakPtr()));

  devtools_client_->GetCSS()->GetExperimental()->GetLayoutTreeAndStyles(
      css::GetLayoutTreeAndStylesParams::Builder()
          .SetComputedStyleWhitelist(css_style_whitelist)
          .Build(),
      base::Bind(&FlatDomTreeExtractor::OnLayoutTreeAndStylesFetched,
                 weak_factory_.GetWeakPtr()));
}

void FlatDomTreeExtractor::OnDocumentFetched(
    std::unique_ptr<dom::GetFlattenedDocumentResult> result) {
  dom_tree_.document_result_ = std::move(result);
  MaybeExtractDomTree();
}

void FlatDomTreeExtractor::OnLayoutTreeAndStylesFetched(
    std::unique_ptr<css::GetLayoutTreeAndStylesResult> result) {
  dom_tree_.layout_tree_and_styles_result_ = std::move(result);
  MaybeExtractDomTree();
}

void FlatDomTreeExtractor::MaybeExtractDomTree() {
  if (dom_tree_.document_result_ && dom_tree_.layout_tree_and_styles_result_) {
    for (const std::unique_ptr<headless::dom::Node>& node :
         *dom_tree_.document_result_->GetNodes()) {
      EnumerateNodes(node.get());
    }
    ExtractLayoutTreeNodes();
    ExtractComputedStyles();
    devtools_client_->GetDOM()->Disable();

    work_in_progress_ = false;

    callback_.Run(std::move(dom_tree_));
  }
}

void FlatDomTreeExtractor::EnumerateNodes(const dom::Node* node) {
  // Allocate an index and record the node pointer.
  size_t index = dom_tree_.node_id_to_index_.size();
  dom_tree_.node_id_to_index_[node->GetNodeId()] = index;
  dom_tree_.dom_nodes_.push_back(node);

  if (node->HasContentDocument())
    EnumerateNodes(node->GetContentDocument());

  DCHECK(!node->HasChildren() || node->GetChildren()->empty());
}

void FlatDomTreeExtractor::ExtractLayoutTreeNodes() {
  dom_tree_.layout_tree_nodes_.reserve(
      dom_tree_.layout_tree_and_styles_result_->GetLayoutTreeNodes()->size());

  for (const std::unique_ptr<css::LayoutTreeNode>& layout_node :
       *dom_tree_.layout_tree_and_styles_result_->GetLayoutTreeNodes()) {
    std::unordered_map<NodeId, size_t>::const_iterator it =
        dom_tree_.node_id_to_index_.find(layout_node->GetNodeId());
    DCHECK(it != dom_tree_.node_id_to_index_.end());
    dom_tree_.layout_tree_nodes_.push_back(layout_node.get());
  }
}

void FlatDomTreeExtractor::ExtractComputedStyles() {
  dom_tree_.computed_styles_.reserve(
      dom_tree_.layout_tree_and_styles_result_->GetComputedStyles()->size());

  for (const std::unique_ptr<css::ComputedStyle>& computed_style :
       *dom_tree_.layout_tree_and_styles_result_->GetComputedStyles()) {
    dom_tree_.computed_styles_.push_back(computed_style.get());
  }
}

FlatDomTreeExtractor::DomTree::DomTree() {}
FlatDomTreeExtractor::DomTree::~DomTree() {}

FlatDomTreeExtractor::DomTree::DomTree(DomTree&& other) = default;

}  // namespace headless
